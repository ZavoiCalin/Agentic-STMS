# Native Jetson runtime

The supported camera application is `../cpp/gst_dual_stream.cpp`. It uses
exactly two CSI-connected Raspberry Pi Camera Module v2 sensors (Argus sensor
IDs 0 and 1), DeepStream `nvinfer` for CUDA-backed inference, and sends each
camera's latest detection count (`object_count`) to the Raspberry Pi once per
second. Confirm the JetPack/Argus camera stack recognizes both IMX219 sensors
before running the app.

Build on the Jetson with the installed JetPack and DeepStream SDK:

```sh
cmake -S ../cpp -B ../build
cmake --build ../build --config Release
../build/stms-jetson-cameras --config /path/to/deepstream-infer-config.txt --pi RASPBERRY_PI_HOST
```

Use `--headless` without a display. Set `STMS_DEEPSTREAM_CONFIG` when launching
through `launch-stms jetson`; set `STMS_PI_HOST` and optionally `STMS_PI_PORT`
(default `5000`) to the Raspberry Pi address. Start `launch-stms receive-jetson`
on the Pi before starting the Jetson pipeline.

The receiver persists JSON Lines to `jetson_data.jsonl` and recomputes the
route. Camera IDs map to intersections A and B unless an event carries an
explicit `intersection`.

`dual_stream_tensorrt.cpp` and `.cu` are experimental prototypes; the supported
runtime is the DeepStream target above.
