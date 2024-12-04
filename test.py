import websocket
import json
import threading
import time

# Replace with your server's URL
ws_url = "ws://192.168.1.4:4398/ws"

# Callback when connection is opened
def on_open(ws):
    print("Connection opened")

    # Start a new thread to send messages
    def run():
        try:
            # Test get_status
            test_get_status(ws)
            time.sleep(1)

            # Test execute_gcode
            test_execute_gcode(ws, "G1 X10 Y10 F1500")
            time.sleep(1)

            # Test file_upload
            # test_file_upload(ws, "test_file.gcode", "G1 X10 Y10\nG1 X20 Y20")
            # time.sleep(1)

            # Test execute_file
            test_execute_file(ws, "test_file.gcode")
            time.sleep(1)

            # Test list_file
            test_list_file(ws)
            time.sleep(1)

            # # Test file_download
            # test_file_download(ws, "test_file.gcode")
            # time.sleep(1)

            # # Test delete_file
            # test_delete_file(ws, "test_file.gcode")
            # time.sleep(1)

            # Close the WebSocket connection
            print("Closing connection...")
            ws.close()
        except Exception as e:
            print(f"Error in run thread: {e}")

    threading.Thread(target=run).start()

# Callback when a message is received
def on_message(ws, message):
    print(f"Received message: {message}")

# Callback when an error occurs
def on_error(ws, error):
    print(f"WebSocket Error: {error}")

# Callback when the connection is closed
def on_close(ws, close_status_code, close_msg):
    print(f"WebSocket closed: {close_status_code}, {close_msg}")

# Function to test get_status
def test_get_status(ws):
    print("Testing get_status")
    message = {
        "type": "get_status"
    }
    ws.send(json.dumps(message))

# Function to test execute_gcode
def test_execute_gcode(ws, gcode_command):
    print(f"Testing execute_gcode with command: {gcode_command}")
    message = {
        "type": "execute_gcode",
        "command": gcode_command
    }
    ws.send(json.dumps(message))

# # Function to test file_upload
# def test_file_upload(ws, filename, file_data):
#     print(f"Testing file_upload for file: {filename}")
#     # For simplicity, we assume the file is small and send it in one chunk
#     chunk_number = 1
#     total_chunks = 1
#     message = {
#         "type": "file_upload",
#         "filename": filename,
#         "chunk_number": chunk_number,
#         "total_chunks": total_chunks,
#         "data": file_data
#     }
#     ws.send(json.dumps(message))

# Function to test execute_file
def test_execute_file(ws, filename):
    print(f"Testing execute_file for file: {filename}")
    message = {
        "type": "execute_file",
        "filename": filename
    }
    ws.send(json.dumps(message))

# Function to test list_file
def test_list_file(ws):
    print("Testing list_file")
    message = {
        "type": "get_list_file"
    }
    ws.send(json.dumps(message))

# Function to test file_download
def test_file_download(ws, filename):
    print(f"Testing file_download for file: {filename}")
    message = {
        "type": "file_download",
        "filename": filename
    }
    ws.send(json.dumps(message))

# Function to test delete_file
def test_delete_file(ws, filename):
    print(f"Testing delete_file for file: {filename}")
    message = {
        "type": "delete_file",
        "filename": filename
    }
    ws.send(json.dumps(message))

# Create a WebSocketApp instance
ws = websocket.WebSocketApp(
    ws_url,
    on_open=on_open,
    on_message=on_message,
    on_error=on_error,
    on_close=on_close,
)

# Run the WebSocket client
ws.run_forever()
