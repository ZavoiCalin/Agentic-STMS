#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <cuda_runtime.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <NvInfer.h>

// Logger for TensorRT info and errors
class TRTLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
} gLogger;

// CUDA Kernel: Preprocesses the frame and handles on-screen overlays
__global__ void preprocessAndDrawKernel(uchar4* surface, float* trtInput, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    
    if (x < width && y < height) {
        int idx = y * width + x;
        uchar4 pixel = surface[idx];
        
        // 1. CUDA Preprocessing: Convert RGBA to Planar RGB and normalize to [0.0, 1.0]
        int channel_size = width * height;
        trtInput[0 * channel_size + idx] = (float)pixel.x / 255.0f; // Red
        trtInput[1 * channel_size + idx] = (float)pixel.y / 255.0f; // Green
        trtInput[2 * channel_size + idx] = (float)pixel.z / 255.0f; // Blue

        // 2. Example UI Draw: Bounding box overlay directly on the zero-copy display frame
        if (x > 100 && x < 300 && y > 100 && y < 110) {
            surface[idx] = make_uchar4(255, 0, 0, pixel.w); // Red top boundary
        }
    }
}

// Helper to load serialized engine file
std::vector<char> loadEngineFile(const std::string& model_path) {
    std::ifstream file(model_path, std::ios::binary | std::ios::ate);
    if (!file.good()) {
        std::cerr << "[-] Failed to open engine file: " << model_path << std::endl;
        return {};
    }
    size_t size = file.tellg();
    std::vector<char> buffer(size);
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), size);
    return buffer;
}

void cameraProcessingWorker(int sensor_id, int width, int height, const std::string& engine_path) {
    // 1. Initialize TensorRT Runtime Context
    std::vector<char> engine_data = loadEngineFile(engine_path);
    if (engine_data.empty()) return;

    nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(gLogger);
    nvinfer1::ICudaEngine* engine = runtime->deserializeCudaEngine(engine_data.data(), engine_data.size());
    nvinfer1::IExecutionContext* context = engine->createExecutionContext();

    if (!context) {
        std::cerr << "[-] Failed to create TensorRT Execution Context for Cam: " << sensor_id << std::endl;
        return;
    }

    // 2. Setup isolated CUDA stream
    cudaStream_t stream;
    cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking);

    // 3. Allocate TensorRT Output Space on Device (Assuming a dummy output size of 1000 classes/box parameters)
    // In a production setup, query engine->getBindingDimensions() to dynamically get sizes
    float* d_trt_input = nullptr;
    float* d_trt_output = nullptr;
    size_t input_size = 3 * width * height * sizeof(float);
    size_t output_size = 1000 * sizeof(float); 

    cudaMalloc(&d_trt_input, input_size);
    cudaMalloc(&d_trt_output, output_size);

    // Array of GPU pointers matching TensorRT engine binding order [Input, Output]
    void* buffers[2] = { d_trt_input, d_trt_output };

    // 4. GStreamer Capture Pipeline (nvarguscamerasrc)
    std::string cap_pipeline_str = 
        "nvarguscamerasrc sensor-id=" + std::to_string(sensor_id) + " bufapi-version=true ! "
        "video/x-raw(memory:NVMM), width=" + std::to_string(width) + ", height=" + std::to_string(height) + ", format=NV12, framerate=30/1 ! "
        "nvvidconv ! video/x-raw(memory:NVMM), format=RGBA ! "
        "appsink name=sink emit-signals=true max-buffers=1 drop=true";

    // 5. GStreamer Display Pipeline (nv3dsink)
    std::string disp_pipeline_str = 
        "appsrc name=source is-live=true format=3 ! "
        "video/x-raw(memory:NVMM), width=" + std::to_string(width) + ", height=" + std::to_string(height) + ", format=RGBA ! "
        "nv3dsink window-x=" + std::to_string(sensor_id * width) + " window-y=0 window-width=" + std::to_string(width) + " window-height=" + std::to_string(height);

    GError* error = nullptr;
    GstElement* cap_pipeline = gst_parse_launch(cap_pipeline_str.c_str(), &error);
    GstElement* disp_pipeline = gst_parse_launch(disp_pipeline_str.c_str(), &error);
    
    GstElement* appsink = gst_bin_get_by_name(GST_BIN(cap_pipeline), "sink");
    GstElement* appsrc = gst_bin_get_by_name(GST_BIN(disp_pipeline), "source");

    gst_element_set_state(cap_pipeline, GST_STATE_PLAYING);
    gst_element_set_state(disp_pipeline, GST_STATE_PLAYING);

    std::cout << "[+] Pipeline " << sensor_id << " initialized (Inference Runtime Linked)." << std::endl;

    // 6. Main Processing Loop
    for (int frame = 0; frame < 300; ++frame) {
        GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
        if (!sample) break;

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;

        if (gst_buffer_map(buffer, &map, GST_MAP_READWRITE)) {
            uchar4* d_surface = (uchar4*)map.data;

            dim3 threadsPerBlock(16, 16);
            dim3 numBlocks((width + threadsPerBlock.x - 1) / threadsPerBlock.x,
                           (height + threadsPerBlock.y - 1) / threadsPerBlock.y);

            // A. Execute Preprocessing Kernel asynchronously on stream
            preprocessAndDrawKernel<<<numBlocks, threadsPerBlock, 0, stream>>>(d_surface, d_trt_input, width, height);

            // B. Non-blocking TensorRT Asynchronous Inference on the identical CUDA stream
            context->enqueueV2(buffers, stream, nullptr);

            // C. Sync stream to ensure Preprocessing + ML Inference finish before display
            cudaStreamSynchronize(stream);

            // D. (Optional) Parse d_trt_output here if you want to draw dynamic bounding boxes on the next frame loop

            gst_buffer_unmap(buffer, &map);
            
            // E. Push zero-copy NVMM buffer straight to hardware renderer
            GstFlowReturn ret;
            g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
        }
        gst_sample_unref(sample);
    }

    // Clean up
    gst_element_set_state(cap_pipeline, GST_STATE_NULL);
    gst_element_set_state(disp_pipeline, GST_STATE_NULL);
    gst_object_unref(appsink); gst_object_unref(appsrc);
    gst_object_unref(cap_pipeline); gst_object_unref(disp_pipeline);
    
    cudaFree(d_trt_input);
    cudaFree(d_trt_output);
    cudaStreamDestroy(stream);
    context->destroy();
    engine->destroy();
    runtime->destroy();
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    int width = 1280;
    int height = 720;
    std::string engine_path = "model.engine"; // Path to your shared or specific model configuration

    std::thread camera0_thread(cameraProcessingWorker, 0, width, height, engine_path);
    std::thread camera1_thread(cameraProcessingWorker, 1, width, height, engine_path);

    camera0_thread.join();
    camera1_thread.join();

    return 0;
}


// compilation with g++

/*
g++ main.cpp -o dual_cam_trt_disp \
    `pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0` \
    -I/usr/include/aarch64-linux-gnu \
    -L/usr/local/cuda/lib64 -lcudart \
    -L/usr/lib/aarch64-linux-gnu -lnvinfer \
    -lpthread
*/


// compilation with nvcc

/*

nvcc main.cu -o dual_cam_trt_disp \
    -O3 \
    -std=c++14 \
    `pkg-config --cflags --libs gstreamer-1.0 gstreamer-app-1.0` \
    -I/usr/include/aarch64-linux-gnu \
    -L/usr/lib/aarch64-linux-gnu \
    -lnvinfer \
    -lpthread
*/


// compilation with cmake

/*
mkdir build && cd build
cmake ..
make -j$(nproc)
*/
