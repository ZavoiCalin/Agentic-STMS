import json
from heapq import heappop, heappush
from pathlib import Path

import numpy as np
from sklearn.ensemble import RandomForestRegressor

BASE_DIR = Path(__file__).resolve().parent
AIR_QUALITY_PATH = BASE_DIR / "sensor_data.json"
TRAFFIC_PATH = BASE_DIR / "intersection_data.json"
OUTPUT_PATH = BASE_DIR / "optimal_route_results.json"

# Road network graph with distances
ROAD_GRAPH = {
    "Intersection A": {"Intersection B": 5, "Intersection C": 10, "Intersection D": 8},
    "Intersection B": {"Intersection A": 5, "Intersection C": 4, "Intersection D": 7},
    "Intersection C": {"Intersection A": 10, "Intersection B": 4, "Intersection D": 6},
    "Intersection D": {"Intersection A": 8, "Intersection B": 7, "Intersection C": 6},
}


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def normalize_air_quality_data(raw_data):
    normalized = []
    for entry in raw_data:
        if not isinstance(entry, dict):
            continue
        if isinstance(entry.get("data"), dict):
            row = dict(entry["data"])
            row["timestamp"] = entry.get("timestamp")
            row["sensorType"] = entry.get("sensorType")
            normalized.append(row)
        else:
            normalized.append(entry)
    return normalized


def calculate_air_quality_penalty(co, no2, pm25, temperature=25.0, humidity=50.0):
    penalty = (co * 0.15) + (no2 * 0.07) + (pm25 * 0.03)
    penalty += (temperature * 0.01)
    penalty += (humidity * 0.01)
    return penalty


def build_feature_vector(traffic_entry, air_data_entry):
    light_status = str(traffic_entry.get("light_status", "green")).lower()
    traffic_volume = float(traffic_entry.get("traffic_volume", 0))
    average_speed = float(traffic_entry.get("average_speed", 0))
    vehicle_count = float(traffic_entry.get("vehicle_count", 0))
    rain = float(traffic_entry.get("rain", 0))
    co = float(air_data_entry.get("co", 0.0))
    no2 = float(air_data_entry.get("no2", 0.0))
    pm25 = float(air_data_entry.get("pm25", 0.0))
    temperature = float(air_data_entry.get("temperature", 25.0))
    humidity = float(air_data_entry.get("humidity", 50.0))
    return [
        traffic_volume,
        average_speed,
        vehicle_count,
        1 if light_status == "red" else 0,
        rain,
        co,
        no2,
        pm25,
        temperature,
        humidity,
    ]


def generate_training_data(traffic_data, air_quality_data):
    X = []
    y = []

    for timestamp, entry in traffic_data.items():
        air_data_entry = next(
            (ad for ad in air_quality_data if str(ad.get("timestamp")) == str(timestamp)),
            {},
        )
        if not air_data_entry:
            air_data_entry = {
                "co": 0.0,
                "no2": 0.0,
                "pm25": 0.0,
                "temperature": 25.0,
                "humidity": 50.0,
            }

        features = build_feature_vector(entry, air_data_entry)
        # Synthetic target based on traffic and air quality severity.
        target = (
            float(entry.get("traffic_volume", 0))
            * 0.35
            + float(entry.get("vehicle_count", 0)) * 0.25
            + calculate_air_quality_penalty(
                air_data_entry.get("co", 0.0),
                air_data_entry.get("no2", 0.0),
                air_data_entry.get("pm25", 0.0),
                air_data_entry.get("temperature", 25.0),
                air_data_entry.get("humidity", 50.0),
            )
        )
        X.append(features)
        y.append(target)

    return np.array(X, dtype=float), np.array(y, dtype=float)


def train_model(traffic_data_dict, air_quality_data):
    X_train, y_train = generate_training_data(traffic_data_dict, air_quality_data)
    if len(X_train) < 2:
        return None
    model = RandomForestRegressor(n_estimators=100, random_state=42)
    model.fit(X_train, y_train)
    return model


def calculate_route_weights(graph, traffic_data, air_quality_data, model):
    route_weights = {}
    for start, neighbors in graph.items():
        traffic_entry = traffic_data.get(start)
        air_data_entry = next(
            (ad for ad in air_quality_data if str(ad.get("timestamp")) == str(start)),
            {},
        )
        if not air_data_entry:
            air_data_entry = {
                "co": 0.0,
                "no2": 0.0,
                "pm25": 0.0,
                "temperature": 25.0,
                "humidity": 50.0,
            }

        for end, base_distance in neighbors.items():
            if traffic_entry is not None and model is not None:
                features = build_feature_vector(traffic_entry, air_data_entry)
                weight = float(model.predict(np.array(features, dtype=float).reshape(1, -1))[0])
                adjusted_weight = max(weight, float(base_distance))
            else:
                adjusted_weight = float(base_distance)
            route_weights[(start, end)] = adjusted_weight
    return route_weights


def find_optimal_route(start, end, graph, route_weights):
    priority_queue = [(0, start)]
    shortest_paths = {node: float("inf") for node in graph}
    shortest_paths[start] = 0
    previous_nodes = {}

    while priority_queue:
        current_weight, current_intersection = heappop(priority_queue)
        if current_intersection == end:
            break

        for neighbor in graph[current_intersection]:
            weight = route_weights.get((current_intersection, neighbor), graph[current_intersection][neighbor])
            new_weight = current_weight + weight
            if new_weight < shortest_paths[neighbor]:
                shortest_paths[neighbor] = new_weight
                previous_nodes[neighbor] = current_intersection
                heappush(priority_queue, (new_weight, neighbor))

    if shortest_paths.get(end, float("inf")) == float("inf"):
        return [start, end], float("inf")

    route = []
    current = end
    while current in previous_nodes:
        route.insert(0, current)
        current = previous_nodes[current]
    route.insert(0, start)
    return route, shortest_paths[end]


def main():
    try:
        air_quality_data = normalize_air_quality_data(load_json(AIR_QUALITY_PATH))
        traffic_data = load_json(TRAFFIC_PATH)
    except FileNotFoundError as exc:
        raise FileNotFoundError(f"Data files are missing from {BASE_DIR}: {exc}") from exc

    traffic_data_dict = {
        entry["timestamp"]: {
            "traffic_volume": entry.get("traffic_volume", 0),
            "average_speed": entry.get("average_speed", 0),
            "vehicle_count": entry.get("vehicle_count", 0),
            "light_status": entry.get("light_status", "green"),
            "rain": entry.get("rain", 0),
        }
        for entry in traffic_data
    }

    model = train_model(traffic_data_dict, air_quality_data)
    route_weights = calculate_route_weights(ROAD_GRAPH, traffic_data_dict, air_quality_data, model)
    start_intersection = "Intersection A"
    end_intersection = "Intersection D"
    optimal_route, total_weight = find_optimal_route(start_intersection, end_intersection, ROAD_GRAPH, route_weights)

    results = {"optimal_route": optimal_route, "total_weight": total_weight}
    print("Optimal Route:", optimal_route)
    print("Total Weight (Cost or Time):", total_weight)

    with OUTPUT_PATH.open("w", encoding="utf-8") as file:
        json.dump(results, file, indent=2)


if __name__ == "__main__":
    main()
