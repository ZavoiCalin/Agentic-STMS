# Azure deployment steps for route processing

## 1. Create an Azure IoT Hub

1. Sign in to the Azure portal.
2. Create a new resource named `stms-iot-hub`.
3. Choose the Standard tier.
4. Enable message routing if needed.
5. Note the IoT Hub connection string and Device Connection String.

## 2. Register the Raspberry Pi device

1. Open IoT Hub > Devices.
2. Add a new device.
3. Save the device connection string.
4. Use it in the Pi client script.

## 3. Create Blob Storage

1. Create a Storage Account.
2. Add a container, for example: `routes`.
3. Keep the account connection string.

## 4. Create a Function App

1. Create a Function App in Azure.
2. Choose Node.js runtime.
3. Add the Event Hub trigger or IoT Hub routing.
4. Set environment variables:
   - `AZURE_STORAGE_CONNECTION_STRING`
   - `AZURE_BLOB_CONTAINER_NAME`

## 5. Deploy the sample function

Upload the code from:

- `MobileApp/backend/azure-function/routeProcessor/index.js`

Set the Azure Function to receive IoT Hub or Event Hub messages.

## 6. Store the processed route

When a route payload arrives, the Function stores the JSON blob in Blob Storage and can then expose it through a backend API.

## 7. Mobile app call

The mobile app should fetch the latest processed route from the backend API, not directly from Blob Storage.

Recommended pattern:

- Pi sends route to IoT Hub
- Function stores route in Blob
- Backend reads the latest blob or database entry
- Frontend requests latest route via the backend endpoint

This keeps the mobile app simple and performance-friendly.
