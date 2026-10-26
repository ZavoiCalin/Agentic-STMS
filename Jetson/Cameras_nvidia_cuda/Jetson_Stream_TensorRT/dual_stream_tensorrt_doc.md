# Native Jetson runtime

The supported camera application is `../cpp/gst_dual_stream.cpp`. It uses
exactly two CSI-connected Raspberry Pi Camera Module v2 sensors (Argus sensor
IDs 0 and 1). It keeps frames in Jetson NVMM surfaces, batches both live
streams for TensorRT `nvinfer`, then uses NVIDIA DeepStream NvDCF multi-object
tracking on the GPU. Vehicle counts are the unique tracked vehicle IDs in each
frame, with detections lacking a track ID counted directly. GPU-mode OSD is
used when displaying; headless mode bypasses OSD to avoid wasted GPU work. Small metadata
messages—not frames—are sent to the Raspberry Pi. This takes advantage of the
Jetson CSI/Argus + ISP + NVMM + CUDA/TensorRT path; it is not a CPU/OpenCV
fallback intended for a general-purpose computer.

Each camera sends one JSON report to the Pi at most once per second, containing
only `timestamp`, `intersection_coordinates`, and `vehicle_count`. Both cameras
currently use the same Timișoara coordinates (latitude `45.7489`, longitude
`21.2087`); the Pi maps this location to Intersection A. Vehicle counts are
based on DeepStream object labels (`car`, `truck`, `bus`,
`motorcycle`/`motorbike`, `van`, or `vehicle`); configure the model's labels
accordingly.
Confirm JetPack/Argus recognizes both IMX219 sensors before running the app.

Develop and build this target directly on the Jetson using that device's
installed JetPack and DeepStream SDK. Clone or update the source on the Jetson;
no executable or CMake build directory from the host PC is required. The
matching CUDA toolkit and DeepStream development files must be installed and
visible through `pkg-config`.

```sh
cmake -S Jetson/Cameras_nvidia_cuda/cpp -B Jetson/Cameras_nvidia_cuda/build -DCMAKE_BUILD_TYPE=Release
cmake --build Jetson/Cameras_nvidia_cuda/build --parallel
Jetson/Cameras_nvidia_cuda/build/stms-jetson-cameras \
  --config /path/to/deepstream-infer-config.txt \
  --tracker-config /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml \
  --pi RASPBERRY_PI_HOST
```

Run those commands from the repository root. Re-run the `cmake --build`
command after source changes to incrementally rebuild on the Jetson. Validate
the runtime on the Jetson with its CSI cameras and DeepStream configuration;
the camera pipeline is hardware- and SDK-specific.

The inference config must set `gpu-id=0`, `batch-size=2`, and
`network-mode=2` (FP16) or `network-mode=1` (INT8). Use an engine optimized
for the deployed model. Prefer FP16 for a good performance/accuracy balance;
use INT8 only with a valid calibration cache. The app logs per-camera
inference/tracker throughput every five seconds. Keep inference `interval=0`
so every frame is passed to the tracker.

Use `--headless` without a display (or set `STMS_JETSON_HEADLESS=1` with the
launcher). Headless mode bypasses the OSD stage. With the launcher set
`STMS_DEEPSTREAM_CONFIG` to the inference config and `STMS_TRACKER_CONFIG` to
the NvDCF performance tracker YAML. The NvDCF library defaults to
`/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so`; override
it with `STMS_TRACKER_LIBRARY` if needed. Set `STMS_PI_HOST` and optionally
`STMS_PI_PORT` (default `5000`) to the Pi. Start `launch-stms receive-jetson`
on the Pi first.

The receiver persists JSON Lines to `jetson_data.jsonl` and recomputes the
route. The configured Timișoara coordinates map to Intersection A. Live camera
data takes precedence over historical sample traffic for the same intersection.

`dual_stream_tensorrt.cpp` and `.cu` are experimental prototypes; the supported
runtime is the DeepStream target above.
