# Route database schema

The backend can store one record per route result. This is the minimal structure for the mobile app to fetch the latest route.

## SQL schema

```sql
CREATE TABLE IF NOT EXISTS routes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_id TEXT NOT NULL,
    route_id TEXT NOT NULL UNIQUE,
    status TEXT NOT NULL DEFAULT 'pending',
    started_at TEXT,
    ended_at TEXT,
    distance_km REAL,
    payload TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_routes_device_id ON routes(device_id);
CREATE INDEX IF NOT EXISTS idx_routes_status ON routes(status);
CREATE INDEX IF NOT EXISTS idx_routes_updated_at ON routes(updated_at DESC);
```

## Why this works

- `route_id` gives a unique identity per route
- `payload` stores the actual route coordinates in JSON text
- `status` lets the backend track processing state
- `updated_at` helps select the newest route quickly

## Example latest-route query

```sql
SELECT payload
FROM routes
WHERE status = 'completed'
ORDER BY updated_at DESC
LIMIT 1;
```

## Example JSON payload

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
