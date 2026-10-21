import json
from pathlib import Path

try:
    from azure.iot.hub import IoTHubRegistryManager
except Exception:  # pragma: no cover - optional dependency for Azure cloud integration
    IoTHubRegistryManager = None

BASE_DIR = Path(__file__).resolve().parent
ROUTE_RESULTS_PATH = BASE_DIR.parent / "AI_routing_algorithm" / "optimal_route_results.json"
CONNECTION_STRING_PATH = BASE_DIR / "primary_connection_string.txt"
DEVICE_ID = "RaspberryPi5"


def load_connection_string(filename="primary_connection_string.txt"):
    path = BASE_DIR / filename
    if not path.exists():
        raise FileNotFoundError(f"Connection string file not found: {path}")

    with path.open("r", encoding="utf-8") as file:
        for line in file:
            if line.startswith("CONNECTION_STRING="):
                return line.split("=", 1)[1].strip()
    raise ValueError(f"Connection string not found in file: {path}")


def send_message_to_device(message):
    if IoTHubRegistryManager is None:
        print("Azure IoT Hub SDK is unavailable in this environment. Local-only execution enabled.")
        return False

    try:
        connection_string = load_connection_string()
    except (FileNotFoundError, ValueError) as exc:
        print(f"Unable to send command: {exc}")
        return False

    try:
        registry_manager = IoTHubRegistryManager(connection_string)
        registry_manager.send_c2d_message(DEVICE_ID, message)
        print(f"Message sent to device {DEVICE_ID}: {message}")
        return True
    except Exception as exc:
        print(f"Error sending message: {exc}")
        return False


def main():
    if not ROUTE_RESULTS_PATH.exists():
        print(f"Route file not found at {ROUTE_RESULTS_PATH}. Using a default route for local execution.")
        optimal_route = ["Intersection A", "Intersection D"]
    else:
        with ROUTE_RESULTS_PATH.open("r", encoding="utf-8") as file:
            optimal_route_data = json.load(file)
            optimal_route = optimal_route_data.get("optimal_route", ["Intersection A", "Intersection D"])

    traffic_light_states = ["Red"] * max(len(optimal_route), 1)
    if optimal_route:
        first_intersection = optimal_route[0]
        for index, intersection_name in enumerate(
            [f"Intersection {chr(ord('A') + i)}" for i in range(len(traffic_light_states))]
        ):
            if intersection_name == first_intersection:
                traffic_light_states[index] = "Green"
                break

    print("Updated Traffic Light States:")
    for i, state in enumerate(traffic_light_states):
        print(f"Intersection {chr(ord('A') + i)}: {state}")

    payload = {"traffic_light_states": traffic_light_states}
    send_message_to_device(json.dumps(payload))


if __name__ == "__main__":
    main()
