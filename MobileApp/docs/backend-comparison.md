# Backend options and route data flow

## Option 1: Raspberry Pi directly to backend

This is the simplest and most direct path for your app.

Flow:

- Raspberry Pi computes the route
- Raspberry Pi sends final coordinates to the backend API
- Backend stores the route in a database or file
- Mobile app reads the latest route via GET /api/routes/latest
- Map displays the polyline

Advantages:

- lower latency
- easier to debug
- easier to deploy in a small project
- no Azure infrastructure required

Disadvantages:

- backend must handle device auth and validation
- scaling requires more infrastructure work
- no built-in enterprise device administration toolset

## Option 2: Azure IoT Hub + Blob Storage to backend

This is better when the system grows large, or when you need enterprise telemetry and storage patterns.

Flow:

- Raspberry Pi sends telemetry to Azure IoT Hub
- IoT Hub forwards messages to Azure Functions or backend services
- Large raw files or logs are stored in Blob Storage
- Backend extracts or normalizes the route
- Mobile app polls a processed route endpoint

Advantages:

- robust scalability and device management
- highly suitable for enterprise or multi-device deployments
- good for large raw telemetry and sensor logs
- mature Azure monitoring and reliability features

Disadvantages:

- more infrastructure and cost
- extra processing hop adds complexity
- slower to prototype than a direct backend solution

## Recommendation

For this project, start with direct Pi-to-backend because:

- you need a mobile route map quickly
- the route logic is already on the Raspberry Pi
- the mobile app only needs a route polyline, not raw data archives

Use Azure IoT Hub + Blob Storage only if you later need:

- many devices
- raw telemetry retention
- centralized Azure-based device management
- enterprise operational monitoring

## Best payload format for the route

Send the processed route only, not raw sensor logs.

Example payload:

```json
{
  "deviceId": "rpi-001",
  "timestamp": "2026-10-21T12:47:00Z",
  "route": [
    { "lat": 37.78825, "lng": -122.4324 },
    { "lat": 37.78925, "lng": -122.4310 },
    { "lat": 37.79050, "lng": -122.4298 },
    { "lat": 37.79200, "lng": -122.4280 }
  ]
}
```

This keeps mobile requests lightweight and performant.
