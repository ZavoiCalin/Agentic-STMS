# STMS Mobile Route App

This folder contains the generated mobile frontend and backend examples for the route map feature.

## Structure

- `frontend/` – React Native mobile app
- `backend/node-express/` – Node.js API example
- `backend/python-fastapi/` – Python API example
- Raspberry Pi route sender – native C++ command in `RaspberryPi/Native/` (`launch-stms send-route`)
- `backend/database/` – route storage SQL schema
- `backend/azure-function/` – Azure IoT Hub and Blob Storage example flow
- `docs/` – architecture, comparison, and deployment notes

## Mobile app behavior

The app requests the latest route from the backend and renders it as a polyline on a map.

## Important note

No dependencies were installed. These files are ready to be added to a project and installed in a normal React Native environment.
