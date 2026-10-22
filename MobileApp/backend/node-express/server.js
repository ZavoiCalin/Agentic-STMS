const express = require('express');
const cors = require('cors');

const app = express();
const port = process.env.PORT || 3000;

const timisoaraBounds = {
  minLat: 45.71,
  maxLat: 45.82,
  minLng: 21.18,
  maxLng: 21.34,
};

const clamp = (value, min, max) => Math.min(Math.max(value, min), max);

const randomBetween = (min, max) => min + (Math.random() * (max - min));

const generateRandomRoute = () => {
  const start = {
    lat: randomBetween(45.72, 45.79),
    lng: randomBetween(21.18, 21.31),
  };

  const direction = randomBetween(0, Math.PI * 2);
  const routeLength = randomBetween(0.02, 0.045);
  const routeVectorLat = Math.cos(direction) * routeLength;
  const routeVectorLng = Math.sin(direction) * routeLength;

  const bendAmplitude = randomBetween(0.004, 0.010);
  const bendDir = direction + (Math.PI / 2) * (Math.random() > 0.5 ? 1 : -1);
  const bendLat = Math.cos(bendDir) * bendAmplitude;
  const bendLng = Math.sin(bendDir) * bendAmplitude;

  const end = {
    lat: clamp(start.lat + routeVectorLat, timisoaraBounds.minLat, timisoaraBounds.maxLat),
    lng: clamp(start.lng + routeVectorLng, timisoaraBounds.minLng, timisoaraBounds.maxLng),
  };

  const quarter = {
    lat: start.lat + routeVectorLat * 0.33 + bendLat * 0.65,
    lng: start.lng + routeVectorLng * 0.33 + bendLng * 0.65,
  };

  const threeQuarter = {
    lat: start.lat + routeVectorLat * 0.67 - bendLat * 0.35,
    lng: start.lng + routeVectorLng * 0.67 - bendLng * 0.35,
  };

  return [
    { lat: clamp(start.lat, timisoaraBounds.minLat, timisoaraBounds.maxLat), lng: clamp(start.lng, timisoaraBounds.minLng, timisoaraBounds.maxLng) },
    { lat: clamp(quarter.lat, timisoaraBounds.minLat, timisoaraBounds.maxLat), lng: clamp(quarter.lng, timisoaraBounds.minLng, timisoaraBounds.maxLng) },
    { lat: clamp(threeQuarter.lat, timisoaraBounds.minLat, timisoaraBounds.maxLat), lng: clamp(threeQuarter.lng, timisoaraBounds.minLng, timisoaraBounds.maxLng) },
    { lat: clamp(end.lat, timisoaraBounds.minLat, timisoaraBounds.maxLat), lng: clamp(end.lng, timisoaraBounds.minLng, timisoaraBounds.maxLng) },
  ];
};

const buildLiveRoute = (seedRoute = generateRandomRoute()) => {
  const routePoints = Array.isArray(seedRoute) && seedRoute.length
    ? seedRoute
    : generateRandomRoute();

  const normalized = routePoints.map((point) => ({
    lat: Number(clamp(point.lat, timisoaraBounds.minLat, timisoaraBounds.maxLat).toFixed(6)),
    lng: Number(clamp(point.lng, timisoaraBounds.minLng, timisoaraBounds.maxLng).toFixed(6)),
  }));

  const distanceKm = Number((2.4 + normalized.length * 0.8 + Math.random() * 1.1).toFixed(1));

  return {
    routeId: `route-${Date.now()}`,
    status: 'in_progress',
    startedAt: new Date(Date.now() - 8 * 60 * 1000).toISOString(),
    endedAt: new Date(Date.now() + 7 * 60 * 1000).toISOString(),
    distanceKm,
    coordinates: normalized,
  };
};

let latestRoute = buildLiveRoute(generateRandomRoute());

app.use(cors());
app.use(express.json());

app.get('/', (req, res) => {
  res.json({
    ok: true,
    service: 'stms-route-backend',
    endpoints: {
      health: '/api/health',
      latestRoute: '/api/routes/latest',
      createRoute: 'POST /api/routes',
    },
  });
});

app.get('/api/health', (req, res) => {
  res.json({ ok: true, service: 'stms-route-backend' });
});

app.get('/api/routes/latest', (req, res) => {
  res.json({ route: latestRoute });
});

app.post('/api/routes', (req, res) => {
  const { deviceId, route, status = 'completed' } = req.body;

  if (!deviceId || !Array.isArray(route) || route.length === 0) {
    return res.status(400).json({ error: 'deviceId and route are required' });
  }

  const normalizedRoute = route
    .map((point) => ({
      lat: Number(point.lat),
      lng: Number(point.lng),
    }))
    .filter((point) => Number.isFinite(point.lat) && Number.isFinite(point.lng));

  if (!normalizedRoute.length) {
    return res.status(400).json({ error: 'route points are invalid' });
  }

  latestRoute = {
    routeId: `route-${Date.now()}`,
    status,
    startedAt: new Date().toISOString(),
    endedAt: new Date(Date.now() + 6 * 60 * 1000).toISOString(),
    distanceKm: Number(((normalizedRoute.length * 0.7) + 1.8).toFixed(1)),
    coordinates: normalizedRoute,
  };

  return res.status(201).json({
    success: true,
    routeId: latestRoute.routeId,
    message: 'Route received from Raspberry Pi',
    route: latestRoute,
  });
});

setInterval(() => {
  latestRoute = buildLiveRoute(generateRandomRoute());
}, 4000);

app.listen(port, () => {
  console.log(`Node backend running at http://localhost:${port}`);
});
