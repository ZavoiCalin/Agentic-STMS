#include <gst/gst.h>
#include <gstnvdsmeta.h>
#include <nvdsmeta.h>

#include <filesystem>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <csignal>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>

namespace {

std::atomic<bool> stopping{false};
std::mutex output_mutex;
std::array<std::atomic<gint64>, 2> last_send_us{};
std::array<std::atomic<std::uint64_t>, 2> frames_per_camera{};
std::atomic<gint64> last_fps_report_us{0};
std::mutex telemetry_mutex;
std::condition_variable telemetry_ready;
std::deque<std::string> telemetry_queue;
constexpr double intersection_latitude = 45.7489;
constexpr double intersection_longitude = 21.2087;

bool is_vehicle(const NvDsObjectMeta& object) {
    std::string label(object.obj_label);
    std::transform(label.begin(), label.end(), label.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return label == "car" || label == "truck" || label == "bus" ||
           label == "motorcycle" || label == "motorbike" || label == "van" ||
           label == "vehicle";
}

struct Options {
    std::string config;
    std::string tracker_config;
    std::string tracker_library = "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so";
    std::string pi_host;
    std::string pi_port = "5000";
    bool display = true;
};

void stop_signal(int) {
    stopping = true;
}

void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " --config INFER_CONFIG --tracker-config NVDCF_CONFIG"
                 " [--tracker-lib NVDCF_LIBRARY] [--pi HOST] [--port PORT] [--headless]\n";
}

bool parse_options(int argc, char** argv, Options& options) {
    if (const char* host = std::getenv("STMS_PI_HOST")) options.pi_host = host;
    if (const char* port = std::getenv("STMS_PI_PORT")) options.pi_port = port;
    if (const char* config = std::getenv("STMS_DEEPSTREAM_CONFIG")) options.config = config;
    if (const char* config = std::getenv("STMS_TRACKER_CONFIG")) options.tracker_config = config;
    if (const char* library = std::getenv("STMS_TRACKER_LIBRARY")) options.tracker_library = library;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument == "--config" || argument == "--tracker-config" ||
             argument == "--tracker-lib" || argument == "--pi" || argument == "--port") &&
            i + 1 < argc) {
            const std::string value = argv[++i];
            if (argument == "--config") options.config = value;
            if (argument == "--tracker-config") options.tracker_config = value;
            if (argument == "--tracker-lib") options.tracker_library = value;
            if (argument == "--pi") options.pi_host = value;
            if (argument == "--port") options.pi_port = value;
        } else if (argument == "--headless") {
            options.display = false;
        } else {
            print_usage(argv[0]);
            return false;
        }
    }
    if (options.config.empty() || options.tracker_config.empty()) {
        print_usage(argv[0]);
        return false;
    }
    if (!std::filesystem::is_regular_file(options.config)) {
        std::cerr << "Inference configuration not found: " << options.config << '\n';
        return false;
    }
    if (!std::filesystem::is_regular_file(options.tracker_config)) {
        std::cerr << "NvDCF tracker configuration not found: " << options.tracker_config << '\n';
        return false;
    }
    if (!std::filesystem::is_regular_file(options.tracker_library)) {
        std::cerr << "DeepStream NvDCF library not found: " << options.tracker_library
                  << " (set STMS_TRACKER_LIBRARY if installed elsewhere).\n";
        return false;
    }
    if (options.pi_host.empty() || options.pi_port.empty()) {
        std::cerr << "Set the Raspberry Pi host and TCP port with --pi/--port or STMS_PI_HOST/STMS_PI_PORT.\n";
        return false;
    }
    std::ifstream inference_config(options.config);
    std::string line;
    bool batch_size_two = false;
    bool gpu_zero = false;
    bool low_precision = false;
    while (std::getline(inference_config, line)) {
        const auto comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        const auto equals = line.find('=');
        if (equals == std::string::npos) continue;
        std::string key = line.substr(0, equals);
        std::string value = line.substr(equals + 1);
        const auto trim = [](std::string& text) {
            const auto first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) {
                text.clear();
                return;
            }
            text = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        };
        trim(key);
        trim(value);
        if (key == "batch-size" && value == "2") batch_size_two = true;
        if (key == "gpu-id" && value == "0") gpu_zero = true;
        if (key == "network-mode" && (value == "1" || value == "2")) low_precision = true;
    }
    if (!batch_size_two) {
        std::cerr << "Inference config must set batch-size=2 for the two-camera pipeline.\n";
        return false;
    }
    if (!gpu_zero) {
        std::cerr << "Inference config must set gpu-id=0 to use the Jetson integrated GPU.\n";
        return false;
    }
    if (!low_precision) {
        std::cerr << "Inference config must select TensorRT FP16 (network-mode=2) or calibrated INT8 (network-mode=1).\n";
        return false;
    }
    std::cout << "Using two-camera TensorRT vehicle detection and GPU NvDCF tracking.\n";
    return true;
}

bool queue_counts(guint camera_id, guint vehicle_count) {
    if (camera_id >= last_send_us.size()) return false;
    const gint64 now_us = g_get_monotonic_time();
    gint64 previous = last_send_us[camera_id].load();
    if (now_us - previous < G_USEC_PER_SEC ||
        !last_send_us[camera_id].compare_exchange_strong(previous, now_us)) {
        return true;
    }

    std::ostringstream payload;
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&now, &utc);
    payload << "{\"timestamp\":\"" << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ")
            << "\",\"intersection_coordinates\":{\"latitude\":" << intersection_latitude
            << ",\"longitude\":" << intersection_longitude
            << "},\"vehicle_count\":" << vehicle_count << "}";
    {
        std::lock_guard<std::mutex> lock(telemetry_mutex);
        if (telemetry_queue.size() >= 32) {
            std::lock_guard<std::mutex> output_lock(output_mutex);
            std::cerr << "Dropping camera telemetry because the Pi send queue is full.\n";
            return false;
        }
        telemetry_queue.push_back(payload.str());
    }
    telemetry_ready.notify_one();
    return true;
}

std::string quote_gst_value(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char character : value) {
        if (character == '\\' || character == '"') escaped.push_back('\\');
        escaped.push_back(character);
    }
    return "\"" + escaped + "\"";
}

bool send_payload(const Options& options, const std::string& message) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    if (getaddrinfo(options.pi_host.c_str(), options.pi_port.c_str(), &hints, &addresses) != 0) {
        std::cerr << "Cannot resolve Raspberry Pi host " << options.pi_host << '\n';
        return false;
    }
    int connection = -1;
    for (auto* address = addresses; address; address = address->ai_next) {
        connection = static_cast<int>(socket(address->ai_family, address->ai_socktype, address->ai_protocol));
        if (connection >= 0 && connect(connection, address->ai_addr, address->ai_addrlen) == 0) break;
        if (connection >= 0) close(connection);
        connection = -1;
    }
    freeaddrinfo(addresses);
    if (connection < 0) {
        std::cerr << "Cannot connect to Raspberry Pi at " << options.pi_host << ':' << options.pi_port << '\n';
        return false;
    }
    std::size_t offset = 0;
    while (offset < message.size()) {
        const auto sent = send(connection, message.data() + offset, message.size() - offset, MSG_NOSIGNAL);
        if (sent <= 0) {
            close(connection);
            std::cerr << "Failed to send camera metadata to Raspberry Pi.\n";
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    close(connection);
    return true;
}

void telemetry_worker(const Options& options) {
    for (;;) {
        std::string payload;
        {
            std::unique_lock<std::mutex> lock(telemetry_mutex);
            telemetry_ready.wait(lock, [] { return stopping.load() || !telemetry_queue.empty(); });
            if (telemetry_queue.empty() && stopping.load()) return;
            payload = std::move(telemetry_queue.front());
            telemetry_queue.pop_front();
        }
        send_payload(options, payload);
    }
}

GstPadProbeReturn metadata_probe(GstPad*, GstPadProbeInfo* info, gpointer user_data) {
    (void)user_data;
    GstBuffer* buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    if (buffer == nullptr) return GST_PAD_PROBE_OK;
    NvDsBatchMeta* batch = gst_buffer_get_nvds_batch_meta(buffer);
    if (batch == nullptr) return GST_PAD_PROBE_OK;

    for (NvDsMetaList* frame_node = batch->frame_meta_list; frame_node != nullptr; frame_node = frame_node->next) {
        auto* frame = static_cast<NvDsFrameMeta*>(frame_node->data);
        if (frame == nullptr) continue;
        if (frame->source_id >= frames_per_camera.size()) continue;
        frames_per_camera[frame->source_id].fetch_add(1, std::memory_order_relaxed);
        guint vehicle_count = 0;
        std::unordered_set<guint64> tracked_vehicle_ids;
        for (NvDsMetaList* object_node = frame->obj_meta_list;
             object_node != nullptr; object_node = object_node->next) {
            const auto* object = static_cast<const NvDsObjectMeta*>(object_node->data);
            if (object != nullptr && is_vehicle(*object)) {
                if (object->object_id == UNTRACKED_OBJECT_ID ||
                    tracked_vehicle_ids.insert(object->object_id).second) {
                    ++vehicle_count;
                }
            }
        }
        queue_counts(frame->source_id, vehicle_count);
    }

    const gint64 now_us = g_get_monotonic_time();
    gint64 previous_report_us = last_fps_report_us.load(std::memory_order_relaxed);
    if (previous_report_us == 0) {
        last_fps_report_us.compare_exchange_strong(previous_report_us, now_us);
    } else if (now_us - previous_report_us >= 5 * G_USEC_PER_SEC &&
               last_fps_report_us.compare_exchange_strong(previous_report_us, now_us)) {
        const double elapsed = static_cast<double>(now_us - previous_report_us) / G_USEC_PER_SEC;
        std::lock_guard<std::mutex> lock(output_mutex);
        for (std::size_t camera = 0; camera < frames_per_camera.size(); ++camera) {
            const auto frames = frames_per_camera[camera].exchange(0, std::memory_order_relaxed);
            std::cout << "Camera " << camera + 1 << " inference FPS: "
                      << static_cast<double>(frames) / elapsed << '\n';
        }
    }
    return GST_PAD_PROBE_OK;
}

std::string make_pipeline(const Options& options) {
    std::ostringstream pipeline;
    pipeline
        << "nvstreammux name=mux batch-size=2 width=1280 height=720 live-source=1 "
           "batched-push-timeout=40000 gpu-id=0 nvbuf-memory-type=0 "
        << "nvarguscamerasrc sensor-id=0 ! "
           "video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1 ! "
           "nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! "
           "queue max-size-buffers=2 max-size-bytes=0 max-size-time=0 leaky=downstream ! mux.sink_0 "
        << "nvarguscamerasrc sensor-id=1 ! "
           "video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1 ! "
           "nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! "
           "queue max-size-buffers=2 max-size-bytes=0 max-size-time=0 leaky=downstream ! mux.sink_1 "
        << "mux. ! nvinfer config-file-path=" << quote_gst_value(options.config)
        << " ! nvtracker name=tracker tracker-width=640 tracker-height=384 gpu-id=0 enable-batch-process=1 "
           "ll-lib-file=" << quote_gst_value(options.tracker_library)
        << " ll-config-file=" << quote_gst_value(options.tracker_config) << " ! ";
    if (options.display) {
        pipeline << "nvdsosd name=osd process-mode=1 ! nveglglessink sync=false";
    } else {
        pipeline << "fakesink sync=false";
    }
    return pipeline.str();
}

}

int main(int argc, char** argv) {
    Options options;
    if (!parse_options(argc, argv, options)) return 2;

    std::signal(SIGINT, stop_signal);
    std::signal(SIGTERM, stop_signal);
    std::signal(SIGPIPE, SIG_IGN);
    gst_init(&argc, &argv);

    GError* error = nullptr;
    const std::string description = make_pipeline(options);
    GstElement* pipeline = gst_parse_launch(description.c_str(), &error);
    if (pipeline == nullptr) {
        std::cerr << "Unable to create DeepStream pipeline: "
                  << (error ? error->message : "unknown error") << '\n';
        if (error) g_error_free(error);
        return 1;
    }
    if (error != nullptr) {
        std::cerr << "Invalid DeepStream pipeline: " << error->message << '\n';
        g_error_free(error);
        gst_object_unref(pipeline);
        return 1;
    }
    GstElement* probe_element = gst_bin_get_by_name(
        GST_BIN(pipeline), options.display ? "osd" : "tracker");
    GstPad* probe_pad = probe_element
        ? gst_element_get_static_pad(probe_element, options.display ? "sink" : "src")
        : nullptr;
    if (probe_pad == nullptr) {
        std::cerr << "Unable to find the DeepStream metadata probe pad.\n";
        if (probe_element) gst_object_unref(probe_element);
        gst_object_unref(pipeline);
        return 1;
    }
    gst_pad_add_probe(probe_pad, GST_PAD_PROBE_TYPE_BUFFER, metadata_probe, &options, nullptr);
    gst_object_unref(probe_pad);
    gst_object_unref(probe_element);

    GstBus* bus = gst_element_get_bus(pipeline);
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Unable to start the Jetson camera pipeline.\n";
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return 1;
    }
    std::thread sender(telemetry_worker, std::cref(options));
    std::cout << "CUDA-backed DeepStream dual-camera inference started.\n";
    bool failed = false;
    while (!stopping.load()) {
        GstMessage* message = gst_bus_timed_pop_filtered(
            bus, 250 * GST_MSECOND,
            static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (message == nullptr) continue;
        if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_ERROR) {
            GError* pipeline_error = nullptr;
            gchar* debug = nullptr;
            gst_message_parse_error(message, &pipeline_error, &debug);
            std::cerr << "Jetson pipeline error: "
                      << (pipeline_error ? pipeline_error->message : "unknown error") << '\n';
            if (debug) std::cerr << debug << '\n';
            if (pipeline_error) g_error_free(pipeline_error);
            g_free(debug);
            failed = true;
        }
        gst_message_unref(message);
        break;
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    stopping = true;
    telemetry_ready.notify_all();
    sender.join();
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    return failed ? 1 : 0;
}
