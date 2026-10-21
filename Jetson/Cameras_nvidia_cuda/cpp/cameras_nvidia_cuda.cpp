#include <gst/gst.h>
#include <glib.h>
#include <iostream>
#include <fstream>
#include <nvds_meta.h>
#include <nvds_meta_schema.h>
#include <nvds_version.h>
#include <nvdsinfer.h>
#include <nvdsinfer_custom_impl.h>
#include <nvdsmeta_schema.h>
#include <azureiot/iothub_device_client_ll.h>
#include <azureiot/iothubtransportmqtt.h>

#define CONNECTION_STRING "Your_Azure_IoT_Hub_Connection_String"

static IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle = nullptr;

static GstPadProbeReturn osd_sink_pad_buffer_probe(GstPad* pad, GstPadProbeInfo* info, gpointer u_data) {
    GstBuffer* gst_buffer = gst_pad_probe_info_get_buffer(info);
    if (!gst_buffer) {
        std::cerr << "Unable to get GstBuffer\n";
        return GST_PAD_PROBE_OK;
    }

    NvDsBatchMeta* batch_meta = gst_buffer_get_nvds_batch_meta(gst_buffer);
    for (NvDsMetaList* l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next) {
        NvDsFrameMeta* frame_meta = (NvDsFrameMeta*)(l_frame->data);
        guint num_rects = frame_meta->num_obj_meta;
        guint frame_number = frame_meta->frame_num;
        std::cout << "Frame Number: " << frame_number << ", Number of Objects: " << num_rects << "\n";
    }

    return GST_PAD_PROBE_OK;
}

static void send_image_to_iothub(const std::string& image_path) {
    std::ifstream file(image_path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open image file\n";
        return;
    }

    std::vector<unsigned char> image_data((std::istreambuf_iterator<char>(file)), {});
    IOTHUB_MESSAGE_HANDLE message_handle = IoTHubMessage_CreateFromByteArray(image_data.data(), image_data.size());

    if (IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, message_handle, nullptr, nullptr) != IOTHUB_CLIENT_OK) {
        std::cerr << "Failed to send message to IoT Hub\n";
    } else {
        std::cout << "Sent image to IoT Hub: " << image_path << "\n";
    }

    IoTHubMessage_Destroy(message_handle);
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    // Azure IoT Hub Client Initialization
    IoTHub_Init();
    device_ll_handle = IoTHubDeviceClient_LL_CreateFromConnectionString(CONNECTION_STRING, MQTT_Protocol);
    if (device_ll_handle == nullptr) {
        std::cerr << "Failed to create IoT Hub client\n";
        return -1;
    }

    // Build GStreamer pipeline
    GstElement* pipeline = gst_parse_launch("nvarguscamerasrc ! nvvideoconvert ! nvdsosd name=osd ! nvvideoconvert ! nvegltransform ! nveglglessink", NULL);
    if (!pipeline) {
        std::cerr << "Failed to create pipeline\n";
        return -1;
    }

    GstElement* osd = gst_bin_get_by_name(GST_BIN(pipeline), "osd");
    GstPad* sinkpad = gst_element_get_static_pad(osd, "sink");
    gst_pad_add_probe(sinkpad, GST_PAD_PROBE_TYPE_BUFFER, osd_sink_pad_buffer_probe, NULL, NULL);
    gst_object_unref(sinkpad);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // Clean up
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    IoTHubDeviceClient_LL_Destroy(device_ll_handle);
    IoTHub_Deinit();

    return 0;
}
