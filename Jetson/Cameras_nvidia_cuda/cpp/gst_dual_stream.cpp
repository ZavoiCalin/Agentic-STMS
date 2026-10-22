#include <gst/gst.h>
#include <gstnvdsmeta.h>
#include <nvdsmeta.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <array>
#include <csignal>
#include <condition_variable>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace {

std::atomic<bool> stopping{false};
std::mutex output_mutex;
std::array<std::atomic<gint64>, 2> last_send_us{};
std::mutex telemetry_mutex;
std::condition_variable telemetry_ready;
std::deque<std::string> telemetry_queue;

struct Options {
    std::string config;
    std::string pi_host = "127.0.0.1";
    std::string pi_port = "5000";
    bool display = true;
};

void stop_signal(int) {
    stopping = true;
}

void print_usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " --config INFER_CONFIG [--pi HOST] [--port PORT] [--headless]\n";
}

bool parse_options(int argc, char** argv, Options& options) {
    if (const char* host = std::getenv("STMS_PI_HOST")) options.pi_host = host;
    if (const char* port = std::getenv("STMS_PI_PORT")) options.pi_port = port;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if ((argument == "--config" || argument == "--pi" || argument == "--port") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (argument == "--config") options.config = value;
            if (argument == "--pi") options.pi_host = value;
            if (argument == "--port") options.pi_port = value;
        } else if (argument == "--headless") {
            options.display = false;
        } else {
            print_usage(argv[0]);
            return false;
        }
    }
    if (options.config.empty()) {
        print_usage(argv[0]);
        return false;
    }
    return true;
}

bool queue_counts(guint frame_number, guint camera_id, guint count) {
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
    payload << "{\"camera_id\":\"Cam" << camera_id + 1
            << "\",\"intersection\":\"Intersection " << static_cast<char>('A' + camera_id)
            << "\",\"frame_number\":" << frame_number
            << ",\"object_count\":" << count
            << ",\"timestamp\":\"" << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ") << "\"}";
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
        const auto sent =         send(connection, message.data() + offset, message.size() - offset, MSG_NOSIGNAL);
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
        const guint count = frame->num_obj_meta;
        {
            std::lock_guard<std::mutex> lock(output_mutex);
            std::cout << "Camera " << frame->source_id + 1 << ", frame " << frame->frame_num
                      << ", detections " << count << '\n';
        }
        queue_counts(frame->frame_num, frame->source_id, count);
    }
    return GST_PAD_PROBE_OK;
}

std::string make_pipeline(const Options& options) {
    std::string quoted_config;
    quoted_config.reserve(options.config.size());
    for (const char character : options.config) {
        if (character == '\\' || character == '"') quoted_config.push_back('\\');
        quoted_config.push_back(character);
    }
    std::ostringstream pipeline;
    pipeline
        << "nvstreammux name=mux batch-size=2 width=1280 height=720 live-source=1 "
           "batched-push-timeout=40000 "
        << "nvarguscamerasrc sensor-id=0 ! "
           "video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1 ! "
           "nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! queue ! mux.sink_0 "
        << "nvarguscamerasrc sensor-id=1 ! "
           "video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1 ! "
           "nvvidconv ! video/x-raw(memory:NVMM),format=NV12 ! queue ! mux.sink_1 "
        << "mux. ! nvinfer config-file-path=\"" << quoted_config << "\""
        << " ! nvdsosd name=osd ! ";
    if (options.display) {
        pipeline << "nveglglessink sync=false";
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
    GstElement* osd = gst_bin_get_by_name(GST_BIN(pipeline), "osd");
    GstPad* sink_pad = osd ? gst_element_get_static_pad(osd, "sink") : nullptr;
    if (sink_pad == nullptr) {
        std::cerr << "Unable to find the DeepStream OSD sink pad.\n";
        if (osd) gst_object_unref(osd);
        gst_object_unref(pipeline);
        return 1;
    }
    gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, metadata_probe, &options, nullptr);
    gst_object_unref(sink_pad);
    gst_object_unref(osd);

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
