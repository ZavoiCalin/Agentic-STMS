import cv2
import time
import json
from azure.iot.device import IoTHubDeviceClient, Message

# Azure IoT Hub connection string
CONNECTION_STRING = "Your_Azure_IoT_Hub_Connection_String"

# Create an IoT Hub client
client = IoTHubDeviceClient.create_from_connection_string(CONNECTION_STRING)

def capture_image():
    cap = cv2.VideoCapture(0)  # Open the default camera
    if not cap.isOpened():
        print("Error: Could not open camera.")
        return None

    ret, frame = cap.read()
    cap.release()
    if ret:
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())
        image_path = f"image_{timestamp}.jpg"
        cv2.imwrite(image_path, frame)
        return image_path
    else:
        print("Error: Could not read frame.")
        return None

def send_image_to_iothub(image_path):
    with open(image_path, "rb") as image_file:
        image_data = image_file.read()
    message = Message(image_data)
    message.content_encoding = "utf-8"
    message.content_type = "application/octet-stream"
    client.send_message(message)
    print(f"Sent image to IoT Hub: {image_path}")

def main():
    while True:
        image_path = capture_image()
        if image_path:
            send_image_to_iothub(image_path)
        time.sleep(60)  # Adjust the frequency as needed

if __name__ == "__main__":
    main()
