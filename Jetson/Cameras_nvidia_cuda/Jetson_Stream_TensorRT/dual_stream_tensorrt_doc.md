# Native Jetson runtime

The supported camera application is `../cpp/gst_dual_stream.cpp`. It uses
exactly two CSI-connected Raspberry Pi Camera Module v2 sensors (Argus sensor
IDs 0 and 1). It keeps frames in Jetson NVMM surfaces, batches both live
streams for TensorRT `nvinfer` on the integrated GPU and runs NVIDIA DeepStream's
NvDCF multi-object tracker on the GPU. GPU-mode OSD is used when displaying;
headless mode bypasses OSD to avoid wasted GPU work. Small metadata
messages—not frames—are sent to the Raspberry Pi. This takes advantage of the
Jetson CSI/Argus + ISP + NVMM + CUDA/TensorRT path; it is not a CPU/OpenCV
fallback intended for a general-purpose computer.

Each camera sends its latest detector count and tracked-object count to the
Pi at most once per second. Tracking IDs reduce count jitter across frames;
the Pi bases route penalties on tracked-object count when available.
Confirm JetPack/Argus recognizes both IMX219 sensors before running the app.

Build on the Jetson with the installed JetPack and DeepStream SDK:

```sh
cmake -S ../cpp -B ../build
cmake --build ../build --config Release
../build/stms-jetson-cameras \
  --config /path/to/deepstream-infer-config.txt \
  --tracker-config /opt/nvidia/deepstream/deepstream/samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml \
  --pi RASPBERRY_PI_HOST
```

The inference config must set `gpu-id=0`, `batch-size=2`, and
`network-mode=2` (FP16) or `network-mode=1` (INT8). Use an engine optimized
for the deployed model. Prefer FP16 for a good performance/accuracy balance;
use INT8 only with a valid calibration cache. Keep inference `interval=0`
when every frame is required for tracking. The app logs per-camera
inference/tracker throughput every five seconds.

Use `--headless` without a display (or set `STMS_JETSON_HEADLESS=1` with the
launcher). Headless mode bypasses the OSD stage. With the launcher set `STMS_DEEPSTREAM_CONFIG`
to the inference config and `STMS_TRACKER_CONFIG` to the NvDCF performance
tracker YAML. The NvDCF library defaults to
`/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so`; override
with `STMS_TRACKER_LIBRARY` if needed. Set `STMS_PI_HOST` and optionally
`STMS_PI_PORT` (default `5000`) to the Pi. Start `launch-stms receive-jetson`
on the Pi first.

The receiver persists JSON Lines to `jetson_data.jsonl` and recomputes the
route. Camera IDs map to intersections A and B unless an event carries an
explicit `intersection`.

`dual_stream_tensorrt.cpp` and `.cu` are experimental prototypes; the supported
runtime is the DeepStream target above.
