import json
import datetime
import time
import os
from azure.iot.device import IoTHubDeviceClient, Message

# === Load IoT connection string ===
def load_connection_string(filename="primary_connection_string.txt"):
    with open(filename, 'r') as file:
        for line in file:
            if line.startswith("CONNECTION_STRING="):
                return line.split("=", 1)[1].strip()

CONNECTION_STRING = load_connection_string()
client = IoTHubDeviceClient.create_from_connection_string(CONNECTION_STRING)

arduino_file = "arduino_data.jsonl"
jetson_file = "jetson_data.jsonl"

arduino_lines_read = 0
jetson_lines_read = 0

arduino_data = {}
jetson_data = {}
sent_windows = set()

# === Normalize timestamp to nearest 10s window ===
def normalize_to_10s_window(ts_str):
    dt = datetime.datetime.strptime(ts_str, "%Y-%m-%d %H:%M:%S")
    bucket = (dt.second // 10) * 10
    normalized = dt.replace(second=bucket)
    return normalized.strftime("%Y-%m-%d %H:%M:%S")

def read_new_lines(file_path, start_line):
    if os.path.exists(file_path):
        with open(file_path, "r") as f:
            lines = f.readlines()
            return lines[start_line:]
    return []

def send_to_hub(data):
    message = Message(json.dumps(data))
    message.content_encoding = "utf-8"
    message.content_type = "application/json"
    client.send_message(message)
    print(f"✅ Sent to Azure IoT Hub: {data}")

print("⏳ Joining data every 10 seconds and sending to Azure IoT Hub. Press Ctrl+C to stop.")

try:
    while True:
        # === Load new Arduino data ===
        new_lines = read_new_lines(arduino_file, arduino_lines_read)
        for line in new_lines:
            try:
                obj = json.loads(line)
                win_key = normalize_to_10s_window(obj["timestamp"])
                arduino_data[win_key] = obj
            except Exception as e:
                print(f"⚠️ Arduino JSON error: {e} | Line: {line.strip()}")
        arduino_lines_read += len(new_lines)

        # === Load new Jetson data ===
        new_lines = read_new_lines(jetson_file, jetson_lines_read)
        for line in new_lines:
            try:
                obj = json.loads(line)
                win_key = normalize_to_10s_window(obj["timestamp"])
                jetson_data[win_key] = obj
            except Exception as e:
                print(f"⚠️ Jetson JSON error: {e} | Line: {line.strip()}")
        jetson_lines_read += len(new_lines)

        # === Join on 10s window ===
        common_windows = set(arduino_data.keys()) & set(jetson_data.keys())
        for win_key in sorted(common_windows):
            if win_key not in sent_windows:
                merged = {
                    "timestamp": win_key,
                    **arduino_data[win_key],
                    **jetson_data[win_key]
                }
                merged["timestamp"] = win_key  # Ensure standard timestamp
                send_to_hub(merged)
                sent_windows.add(win_key)

        time.sleep(2)

except KeyboardInterrupt:
    print("\n🛑 Stopped by user.")
finally:
    client.shutdown()
    print("🔌 Disconnected from Azure IoT Hub.")
