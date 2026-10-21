import socket
import json

HOST = ''  # Listen on all interfaces
PORT = 5000
output_file = "jetson_data.jsonl"  # JSON Lines format

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind((HOST, PORT))
server_socket.listen()

print(f"Listening on port {PORT} for Jetson data...")

try:
    while True:
        conn, addr = server_socket.accept()
        with conn:
            print(f"Connected by {addr}")
            data = conn.recv(1024)
            if data:
                try:
                    json_data = data.decode('utf-8')
                    obj = json.loads(json_data)
                    with open(output_file, "a") as f:
                        json.dump(obj, f)
                        f.write("\n")
                    print(f"Saved Jetson data: {obj}")
                except json.JSONDecodeError:
                    print(f"Invalid JSON received: {json_data}")

except KeyboardInterrupt:
    print("\nStopped by user.")
finally:
    server_socket.close()
    print("Socket closed.")
