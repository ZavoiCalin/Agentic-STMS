from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import List

app = FastAPI(title='STMS Route API')


class RoutePoint(BaseModel):
    lat: float
    lng: float


class RoutePayload(BaseModel):
    deviceId: str
    route: List[RoutePoint]


@app.get('/api/routes/latest')
async def get_latest_route():
    return {
        'route': {
            'routeId': 'route-2026-10-21-01',
            'status': 'completed',
            'startedAt': '2026-10-21T12:40:00Z',
            'endedAt': '2026-10-21T12:47:00Z',
            'distanceKm': 4.6,
            'coordinates': [
                {'lat': 37.78825, 'lng': -122.4324},
                {'lat': 37.78925, 'lng': -122.4310},
                {'lat': 37.79050, 'lng': -122.4298},
                {'lat': 37.79200, 'lng': -122.4280},
            ],
        }
    }


@app.post('/api/routes')
async def receive_route(payload: RoutePayload):
    if not payload.deviceId or not payload.route:
        raise HTTPException(status_code=400, detail='deviceId and route are required')

    return {
        'success': True,
        'routeId': 'route-generated-from-pi',
        'message': 'Route received from Raspberry Pi',
    }
