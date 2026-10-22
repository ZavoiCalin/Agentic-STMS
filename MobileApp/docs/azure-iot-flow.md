# Azure IoT Hub + Blob Storage route flow

## Data flow

1. Raspberry Pi computes the route or sends telemetry.
2. The Pi publishes the telemetry data to Azure IoT Hub.
3. Azure IoT Hub routes the message to an Azure Function.
4. The Azure Function validates the route structure.
5. The Function stores the processed route payload in Blob Storage for raw or processed archive use.
6. The backend API exposes the latest route to the React Native app.
7. The mobile app displays the route as a polyline on a map.

## Example IoT Hub payload

```json
{
  "deviceId": "rpi-001",
  "timestamp": "2026-10-21T12:47:00Z",
  "status": "completed",
  "distanceKm": 4.6,
  "route": [
    { "lat": 37.78825, "lng": -122.4324 },
    { "lat": 37.78925, "lng": -122.4310 },
    { "lat": 37.79050, "lng": -122.4298 },
    { "lat": 37.79200, "lng": -122.4280 }
  ]
}
```

## Why this pattern is useful

- scalable for many devices
- centralized device management
- good for storing raw telemetry and blob archives
- easier to keep audit records and device logs

## Why direct Pi-to-backend is still better in many cases

- lower latency
- simpler to build
- no Azure complexity for a smaller deployment
- easier for a prototype or regional deployment

## Recommended solution for your project

Use direct Pi-to-backend for the main route API and mobile app.

Use Azure IoT Hub + Blob Storage if you later need:

- large numbers of edge devices
- secure Azure-based device management
- raw telemetry retention
- centralized cloud observability

## Azure Function summary

The sample function in `backend/azure-function/routeProcessor/index.js` shows the concept of:

- receiving an IoT Hub message
- validating the route payload
- storing a processed route blob in Azure Blob Storage
- preparing the route for downstream API consumption
