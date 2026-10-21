import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
import pyds
import time
import json
from azure.iot.device import IoTHubDeviceClient, Message

# Azure IoT Hub connection string
CONNECTION_STRING = "Your_Azure_IoT_Hub_Connection_String"

# Create an IoT Hub client
client = IoTHubDeviceClient.create_from_connection_string(CONNECTION_STRING)

def osd_sink_pad_buffer_probe(pad, info, u_data):
    frame_number = 0
    num_rects = 0
    gst_buffer = info.get_buffer()
    if not gst_buffer:
        print("Unable to get GstBuffer")
        return

    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(gst_buffer))
    l_frame = batch_meta.frame_meta_list
    while l_frame is not None:
        try:
            frame_meta = pyds.NvDsFrameMeta.cast(l_frame.data)
        except StopIteration:
            break

        frame_number = frame_meta.frame_num
        num_rects = frame_meta.num_obj_meta
        l_frame = l_frame.next

    print(f"Frame Number: {frame_number}, Number of Objects: {num_rects}")
    return Gst.PadProbeReturn.OK

def main():
    Gst.init(None)
    pipeline = Gst.parse_launch("nvarguscamerasrc ! nvvideoconvert ! nvdsosd ! nvvideoconvert ! nvegltransform ! nveglglessink")

    osdsinkpad = pipeline.get_by_name("nvdsosd").get_static_pad("sink")
    if not osdsinkpad:
        print("Unable to get sink pad")
    else:
        osdsinkpad.add_probe(Gst.PadProbeType.BUFFER, osd_sink_pad_buffer_probe, 0)

    pipeline.set_state(Gst.State.PLAYING)
    try:
        loop = GLib.MainLoop()
        loop.run()
    except:
        pass

    pipeline.set_state(Gst.State.NULL)

def send_image_to_iothub(image_path):
    with open(image_path, "rb") as image_file:
        image_data = image_file.read()
    message = Message(image_data)
    message.content_encoding = "utf-8"
    message.content_type = "application/octet-stream"
    client.send_message(message)
    print(f"Sent image to IoT Hub: {image_path}")

if __name__ == "__main__":
    main()
