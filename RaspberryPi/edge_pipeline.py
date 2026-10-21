import argparse
import json
from datetime import datetime
from pathlib import Path

try:
    from pymongo import MongoClient, errors
except Exception:  # pragma: no cover - legacy compatibility only
    MongoClient = None
    errors = None

try:
    from azure.iot.device import IoTHubDeviceClient, Message
except Exception:  # pragma: no cover - optional Azure dependency
    IoTHubDeviceClient = None
    Message = None

try:
    from azure.storage.blob import BlobServiceClient
except Exception:  # pragma: no cover - optional cloud dependency
    BlobServiceClient = None

BASE_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = BASE_DIR.parent
ROUTE_SCRIPT = BASE_DIR / "AI_routing_algorithm" / "ai_routing.py"
TRAFFIC_SCRIPT = BASE_DIR / "Traffic_light_commands" / "main.py"
ROUTE_RESULTS_PATH = BASE_DIR / "AI_routing_algorithm" / "optimal_route_results.json"
SENSOR_DATA_PATH = BASE_DIR / "AI_routing_algorithm" / "sensor_data.json"
AZURE_CONNECTION_PATH = BASE_DIR / "SendData" / "primary_connection_string.txt"
MONGO_CONNECTION_PATH = BASE_DIR / "mongo_connection.txt"
BLOB_CONNECTION_PATH = BASE_DIR / "blob_connection.txt"


def load_connection_string(path: Path):
    if not path.exists():
        return None
    with path.open("r", encoding="utf-8") as file:
        for line in file:
            if line.startswith("CONNECTION_STRING="):
                return line.split("=", 1)[1].strip()
    return None


def load_mongo_settings(path: Path):
    if not path.exists():
        return None
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip() and not line.strip().startswith("#")]
    if len(lines) < 2:
        return None
    return {"connection": lines[0], "database": lines[1]}


def load_blob_settings(path: Path):
    if not path.exists():
        return None
    lines = [line.strip() for line in path.read_text(encoding="utf-8").splitlines() if line.strip() and not line.strip().startswith("#")]
    if len(lines) < 3:
        return None
    return {
        "account_name": lines[0],
        "account_key": lines[1],
        "container_name": lines[2],
    }


def send_to_azure(payload, connection_string=None, device_id="RaspberryPi5"):
    if IoTHubDeviceClient is None or Message is None:
        print("Azure SDK not available; skipping Azure send.")
        return False
    if connection_string is None:
        connection_string = load_connection_string(AZURE_CONNECTION_PATH)
    if not connection_string:
        print("Azure connection string missing; skipping Azure send.")
        return False

    try:
        client = IoTHubDeviceClient.create_from_connection_string(connection_string)
        msg = Message(json.dumps(payload))
        msg.content_encoding = "utf-8"
        msg.content_type = "application/json"
        client.send_message(msg)
        print(f"Sent to Azure IoT Hub: {payload}")
        client.shutdown()
        return True
    except Exception as exc:  # pragma: no cover - cloud transport can fail in offline mode
        print(f"Azure send failed: {exc}")
        return False


def upload_json_to_mongo(collection_name, json_file_path, connection_file_path=MONGO_CONNECTION_PATH):
    if MongoClient is None or errors is None:
        print("pymongo is not installed; skipping MongoDB upload.")
        return False

    settings = load_mongo_settings(connection_file_path)
    if settings is None:
        print(f"MongoDB config not found at {connection_file_path}; skipping Mongo upload.")
        return False

    file_path = Path(json_file_path)
    if not file_path.exists():
        print(f"Mongo payload not found: {file_path}")
        return False

    try:
        client = MongoClient(settings["connection"], serverSelectionTimeoutMS=5000)
        client.admin.command("ping")
        db = client[settings["database"]]
        collection = db[collection_name]
        with file_path.open("r", encoding="utf-8") as file:
            data = json.load(file)
        if isinstance(data, list):
            collection.insert_many(data)
        else:
            collection.insert_one(data)
        print(f"Uploaded {file_path} to MongoDB collection '{collection_name}'.")
        return True
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as exc:
        print(f"MongoDB upload failed: {exc}")
        return False
    except Exception as exc:  # pragma: no cover - connection layer may fail in offline mode
        print(f"MongoDB upload failed: {exc}")
        return False


def upload_json_to_blob(json_file_path, connection_file_path=BLOB_CONNECTION_PATH):
    if BlobServiceClient is None:
        print("azure-storage-blob is not installed; skipping Azure Blob upload.")
        return False

    settings = load_blob_settings(connection_file_path)
    if settings is None:
        print(f"Blob config not found at {connection_file_path}; skipping Azure Blob upload.")
        return False

    file_path = Path(json_file_path)
    if not file_path.exists():
        print(f"Blob payload not found: {file_path}")
        return False

    try:
        account = settings["account_name"]
        key = settings["account_key"]
        container = settings["container_name"]
        service = BlobServiceClient(account_url=f"https://{account}.blob.core.windows.net", credential=key)
        container_client = service.get_container_client(container)
        timestamp = datetime.utcnow().strftime("%Y/%m/%d")
        blob_name = f"{timestamp}/{file_path.name}"
        with file_path.open("rb") as payload:
            container_client.upload_blob(name=blob_name, data=payload, overwrite=True)
        print(f"Uploaded {file_path} to Azure Blob '{container}/{blob_name}'.")
        return True
    except Exception as exc:  # pragma: no cover - cloud transport may fail in offline mode
        print(f"Azure Blob upload failed: {exc}")
        return False


def run_python_script(script_path: Path):
    import subprocess
    import sys

    subprocess.run([sys.executable, str(script_path)], check=True)


def collect_route_payload():
    if not ROUTE_RESULTS_PATH.exists():
        return None
    with ROUTE_RESULTS_PATH.open("r", encoding="utf-8") as file:
        return json.load(file)


def sync_cloud_data(use_legacy_mongo=False):
    route_payload = collect_route_payload()
    if route_payload:
        send_to_azure({"type": "optimal_route", **route_payload})
        upload_json_to_blob(ROUTE_RESULTS_PATH)
        if use_legacy_mongo:
            upload_json_to_mongo("OptimalRouteResults", ROUTE_RESULTS_PATH)

    if SENSOR_DATA_PATH.exists():
        upload_json_to_blob(SENSOR_DATA_PATH)
        if use_legacy_mongo:
            upload_json_to_mongo("SensorReadings", SENSOR_DATA_PATH)


def parse_args():
    parser = argparse.ArgumentParser(description="Hardware-first traffic orchestration matching the project's priority architecture")
    parser.add_argument("--route", action="store_true", help="Run the route computation script")
    parser.add_argument("--traffic", action="store_true", help="Run the traffic-light decision script")
    parser.add_argument("--sync-cloud", action="store_true", help="Upload the latest route/sensor data to Azure IoT Hub and Azure Blob Storage; MongoDB is legacy compatibility only")
    parser.add_argument("--legacy-mongo", action="store_true", help="Optional legacy MongoDB upload for older web-app compatibility")
    parser.add_argument("--jetson-cameras", action="store_true", help="Launch the dual Jetson camera CUDA stream from the Extra/Edge_AI flow")
    parser.add_argument("--all", action="store_true", help="Run route, traffic, cloud sync, and the optional Jetson camera task")
    return parser.parse_args()


def run_optional_executable(path: Path):
    if not path.exists():
        print(f"Optional hardware helper not found: {path}. Skipping.")
        return
    if path.suffix.lower() == ".py":
        run_python_script(path)
    else:
        import os
        if os.name == "nt":
            print(f"Hardware helper {path} is a native executable; only run on the target Jetson hardware.")
        else:
            import subprocess
            subprocess.run([str(path)], check=True)


def main():
    args = parse_args()

    if args.route or args.all:
        run_python_script(ROUTE_SCRIPT)
    if args.traffic or args.all:
        run_python_script(TRAFFIC_SCRIPT)
    if args.sync_cloud or args.all:
        sync_cloud_data(use_legacy_mongo=args.legacy_mongo)
    if args.jetson_cameras or args.all:
        run_optional_executable(PROJECT_ROOT / "Extra" / "Edge_AI" / "build" / "dual_camera_cuda")

    if not any([args.route, args.traffic, args.sync_cloud, args.legacy_mongo, args.jetson_cameras, args.all]):
        print("No action selected. Use --route, --traffic, --sync-cloud, --legacy-mongo, --jetson-cameras, or --all.")


if __name__ == "__main__":
    main()
