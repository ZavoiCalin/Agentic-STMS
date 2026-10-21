from azure.storage.blob import ContainerClient
import fastavro
import io
import json

# Azure Storage config
STORAGE_ACCOUNT_NAME = "<your-storage-account-name>"
STORAGE_ACCOUNT_KEY = "<your-storage-account-key>"
CONTAINER_NAME = "<your-container-name>"
BLOB_PREFIX = ""  # Optional: "2025/05/"

JSON_OUTPUT = "/home/youruser/iot_project/iot_data_latest.json"

blob_url = f"https://{STORAGE_ACCOUNT_NAME}.blob.core.windows.net"
container_client = ContainerClient(
    account_url=blob_url,
    container_name=CONTAINER_NAME,
    credential=STORAGE_ACCOUNT_KEY
)

all_records = []

for blob in container_client.list_blobs(name_starts_with=BLOB_PREFIX):
    if not blob.name.endswith(".avro"):
        continue

    blob_data = container_client.get_blob_client(blob).download_blob().readall()

    with io.BytesIO(blob_data) as avro_file:
        try:
            reader = fastavro.reader(avro_file)
            for record in reader:
                all_records.append(record)
        except Exception as e:
            print(f"⚠️ Error in {blob.name}: {e}")

with open(JSON_OUTPUT, "w", encoding="utf-8") as f:
    json.dump(all_records, f, indent=2)

print(f"✅ Exported {len(all_records)} records to {JSON_OUTPUT}")
