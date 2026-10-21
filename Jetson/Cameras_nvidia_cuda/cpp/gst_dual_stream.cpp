#include <gst/gst.h>

int main(int argc, char *argv[]) {
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_pipeline_new("traffic-pipeline");

    // Sources
    GstElement *cam0 = gst_element_factory_make("nvarguscamerasrc", "cam0");
    GstElement *cam1 = gst_element_factory_make("nvarguscamerasrc", "cam1");

    g_object_set(cam0, "sensor-id", 0, NULL);
    g_object_set(cam1, "sensor-id", 1, NULL);

    // Stream muxer
    GstElement *mux = gst_element_factory_make("nvstreammux", "mux");

    // Inference
    GstElement *infer = gst_element_factory_make("nvinfer", "yolo");

    // On-screen display
    GstElement *osd = gst_element_factory_make("nvdsosd", "osd");

    // Display sink
    GstElement *sink = gst_element_factory_make("nveglglessink", "sink");

    // Add elements
    gst_bin_add_many(GST_BIN(pipeline),
                     cam0, cam1, mux, infer, osd, sink, NULL);

    // NOTE: full linking requires pads (DeepStream standard pattern)

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    g_print("Running...\n");

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}
