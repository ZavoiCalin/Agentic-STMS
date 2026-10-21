#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>

// CUDA Headers
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

// GStreamer Headers
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

// TensorRT Headers
#include <NvInfer.h>

// CUDA Error Checking Macro
#define gpuErrchk(ans) { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char *file, int line, bool abort=true) {
   if (code != cudaSuccess) {
      fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
      if (abort) exit(code);
   }
}

// Logger class for TensorRT
class TRTLogger : public nvinfer1::ILogger {
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cout << "[TensorRT] " << msg << std::endl;
        }
    }
} gLogger;

// CUDA Kernel: Runs on the GPU to preprocess and draw directly on the frame buffer
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

        // 2. Example UI Draw: Draw a bounding box directly on the zero-copy buffer
        if (x > 100 && x < 300 && y > 100 && y < 110) {
            surface[idx] = make_uchar4(255, 0, 0, pixel.w); // Red horizontal line
        }
    }
}

// Helper to load the engine file
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
    // 1. Deserialize TensorRT Engine
    std::vector<char> engine_data = loadEngineFile(engine_path);
    if (engine_data.empty()) return;

    nvinfer1::IRuntime* runtime = nvinfer1::createInferRuntime(gLogger);
    nvinfer1::ICudaEngine* engine = runtime->deserializeCudaEngine(engine_data.data(), engine_data.size());
    nvinfer1::IExecutionContext* context = engine->createExecutionContext();

    if (!context) {
        std::cerr << "[-] Failed to create TRT Execution Context for Cam: " << sensor_id << std::endl;
        return;
    }

    // 2. Setup isolated CUDA stream
    cudaStream_t stream;
    gpuErrchk(cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking));

    // 3. Allocate CUDA Device Memory for TensorRT
    float* d_trt_input = nullptr;
    float* d_trt_output = nullptr;
    size_t input_size = 3 * width * height * sizeof(float);
    size_t output_size = 1000 * sizeof(float); // Adjust to your model's actual output size

    gpuErrchk(cudaMalloc(&d_trt_input, input_size));
    gpuErrchk(cudaMalloc(&d_trt_output, output_size));

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

    std::cout << "[+] Pipeline " << sensor_id << " active with CUDA Stream orchestration." << std::endl;

    // 6. Processing Loop
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

            // A. Launch CUDA Preprocessing Kernel on custom stream
            preprocessAndDrawKernel<<<numBlocks, threadsPerBlock, 0, stream>>>(d_surface, d_trt_input, width, height);
            gpuErrchk(cudaGetLastError()); // Check for kernel launch errors

            // B. Run Async TensorRT Inference on identical stream
            context->enqueueV2(buffers, stream, nullptr);

            // C. Sync stream to block until GPU operations are finished
            gpuErrchk(cudaStreamSynchronize(stream));

            gst_buffer_unmap(buffer, &map);
            
            // D. Push back to display sink
            GstFlowReturn ret;
            g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
        }
        gst_sample_unref(sample);
    }

    // Cleanup
    gst_element_set_state(cap_pipeline, GST_STATE_NULL);
    gst_element_set_state(disp_pipeline, GST_STATE_NULL);
    gst_object_unref(appsink); gst_object_unref(appsrc);
    gst_object_unref(cap_pipeline); gst_object_unref(disp_pipeline);
    
    gpuErrchk(cudaFree(d_trt_input));
    gpuErrchk(cudaFree(d_trt_output));
    gpuErrchk(cudaStreamDestroy(stream));
    context->destroy();
    engine->destroy();
    runtime->destroy();
}

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);

    int width = 1280;
    int height = 720;
    std::string engine_path = "model.engine";

    std::thread camera0_thread(cameraProcessingWorker, 0, width, height, engine_path);
    std::thread camera1_thread(cameraProcessingWorker, 1, width, height, engine_path);

    camera0_thread.join();
    camera1_thread.join();

    return 0;
}