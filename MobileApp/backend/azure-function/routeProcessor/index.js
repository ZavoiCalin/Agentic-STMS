const { BlobServiceClient } = require('@azure/storage-blob');

module.exports = async function (context, eventHubMessages) {
  const messages = Array.isArray(eventHubMessages) ? eventHubMessages : [eventHubMessages];

  for (const message of messages) {
    const payload = typeof message === 'string' ? JSON.parse(message) : message;

    context.log('Received device message from IoT Hub:', JSON.stringify(payload));

    if (!payload || !payload.route || !Array.isArray(payload.route)) {
      context.log('Message missing route data. Skipping.');
      continue;
    }

    const routeSummary = {
      deviceId: payload.deviceId || 'unknown-device',
      timestamp: payload.timestamp || new Date().toISOString(),
      routeId: `route-${Date.now()}`,
      coordinates: payload.route,
      distanceKm: payload.distanceKm || null,
      status: payload.status || 'processed',
    };

    context.bindings.outputRoute = JSON.stringify(routeSummary);

    if (process.env.AZURE_STORAGE_CONNECTION_STRING && process.env.AZURE_BLOB_CONTAINER_NAME) {
      try {
        const blobServiceClient = BlobServiceClient.fromConnectionString(
          process.env.AZURE_STORAGE_CONNECTION_STRING
        );

        const containerClient = blobServiceClient.getContainerClient(
          process.env.AZURE_BLOB_CONTAINER_NAME
        );

        const fileName = `routes/${routeSummary.routeId}.json`;
        const data = JSON.stringify(routeSummary, null, 2);
        const blockBlobClient = containerClient.getBlockBlobClient(fileName);

        await blockBlobClient.upload(data, Buffer.byteLength(data));

        context.log(`Stored processed route in Blob Storage: ${fileName}`);
      } catch (error) {
        context.log.error('Failed to store route blob:', error.message);
      }
    }
  }
};
