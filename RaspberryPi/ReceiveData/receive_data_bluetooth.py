import serial
import json

port_name = "/dev/rfcomm0"
baud_rate = 9600
output_file = "arduino_data.jsonl"  # JSON Lines format: one JSON object per line

try:
    ser = serial.Serial(port_name, baud_rate, timeout=1)
    print(f"Connected to Bluetooth device on {port_name}")
except Exception as e:
    print(f"Failed to connect on {port_name}: {e}")
    ser = None

print("Receiving Arduino data. Press Ctrl+C to stop.")

try:
    with open(output_file, "a") as f:
        while True:
            if ser and ser.in_waiting > 0:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
                if line:
                    try:
                        data = json.loads(line)
                        json.dump(data, f)
                        f.write("\n")
                        f.flush()
                        print(f"Saved Arduino data: {data}")
                    except json.JSONDecodeError:
                        print(f"Invalid JSON from Arduino: {line}")

except KeyboardInterrupt:
    print("\nStopped by user.")
finally:
    if ser:
        ser.close()
    print("Serial connection closed.")
