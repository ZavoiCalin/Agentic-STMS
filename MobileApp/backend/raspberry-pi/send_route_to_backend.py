import json
import os
import time
from urllib import request, error

BACKEND_URL = os.getenv('STMS_BACKEND_URL', 'http://localhost:3000/api/routes')
DEVICE_ID = os.getenv('STMS_DEVICE_ID', 'rpi-001')

SAMPLE_ROUTE = [
    {"lat": 37.78825, "lng": -122.4324},
    {"lat": 37.78925, "lng": -122.4310},
    {"lat": 37.79050, "lng": -122.4298},
    {"lat": 37.79200, "lng": -122.4280},
]


def build_payload(route):
    return {
        "deviceId": DEVICE_ID,
        "timestamp": time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
        "status": "completed",
        "distanceKm": 4.6,
        "route": route,
    }


def send_route(route):
    payload = build_payload(route)
    data = json.dumps(payload).encode('utf-8')

    req = request.Request(
        BACKEND_URL,
        data=data,
        headers={
            'Content-Type': 'application/json',
            'Accept': 'application/json',
        },
        method='POST',
    )

    try:
        with request.urlopen(req, timeout=10) as response:
            body = response.read().decode('utf-8')
            print('Route sent successfully.')
            print(body)
            return True
    except error.HTTPError as exc:
        print(f'HTTP error: {exc.code} {exc.read().decode("utf-8", errors="replace")}')
        return False
    except Exception as exc:
        print(f'Unexpected error sending route: {exc}')
        return False


if __name__ == '__main__':
    print(f'Sending route to {BACKEND_URL}')
    send_route(SAMPLE_ROUTE)
