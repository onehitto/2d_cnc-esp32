import websocket
import json
import threading
import time
from queue import Queue,Empty  

# Replace with your server's URL
ws_url = "ws://192.168.1.4:4398/ws"

response_queue = Queue()

# Callback when connection is opened
def on_open(ws):
    print("Connection opened")

    # Start a new thread to send messages
    def run():
        try:
            # Test get_status
            # test_get_status(ws)
            # time.sleep(1)

            # # Test execute_gcode
            # test_exe_gcode(ws, "G1 X10 Y10 F1500")
            # time.sleep(1)

            # # Test list_file
            # test_config(ws,"&0 &1 &2")
            # time.sleep(1)

            # test_pause(ws)
            # time.sleep(1)

            # test_stop(ws)
            # time.sleep(1)

            # test_resume(ws)
            # time.sleep(1)

            # # Test file_upload
            test_file_upload(ws,".\coord.gcode")
            time.sleep(1)

            test_run_file(ws)
            time.sleep(1)



            # Close the WebSocket connection
            print("Closing connection...")
            ws.close()
        except Exception as e:
            print(f"Error in run thread: {e}")

    threading.Thread(target=run).start()

    

def on_message(ws, message):
    """Callback to handle messages from the server."""
    response = json.loads(message)
    response_queue.put((response, time.time()))

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
def test_exe_gcode(ws, gcode_command):
    print(f"Testing execute_gcode with command: {gcode_command}")
    message = {
        "type": "exe_gcode",
        "data": gcode_command
    }
    ws.send(json.dumps(message))

# # Function to test file_upload
def test_file_upload(ws, filename):
    print(f"Testing file_upload for file: {filename}")
    with open(filename, "r") as f:
        file_data = f.read()
    chunk_size = 2000
    total_chunks = (len(file_data) + chunk_size - 1) // chunk_size

    for chunk_number in range(1, total_chunks + 1):
        start_index = (chunk_number - 1) * chunk_size
        end_index = start_index + chunk_size
        chunk_data = file_data[start_index:end_index]

        # Prepare the message
        message = {
            "type": "upload_file",
            "chunk_number": chunk_number,
            "total_chunks": total_chunks,
            "data": chunk_data
        }
        # Send the message via WebSocket
        send_time = time.time()
        ws.send(json.dumps(message))
        print(f"Sent chunk {chunk_number}/{total_chunks}")
        try:
            response,response_time = response_queue.get(timeout=10)  # Wait for up to 10 seconds
            time_taken = response_time - send_time
            print(f"Time taken for chunk {chunk_number}: {time_taken:.3f}")
        except Queue.Empty:
            print(f"Error: No response received for chunk {chunk_number}")
            break

         

# Function to test list_file
def test_config(ws,config):
    print("Testing config")
    message = {
        "type": "config",
        "data": config
    }
    ws.send(json.dumps(message))
# Function to test execute_file
def test_run_file(ws):
    print(f"Testing run_file")
    message = {
        "type": "run_file",
    }
    ws.send(json.dumps(message))

def test_pause(ws):
    print(f"Testing pause")
    message = {
        "type": "pause",
    }
    ws.send(json.dumps(message))

def test_resume(ws):
    print(f"Testing resume")
    message = {
        "type": "resume",
    }
    ws.send(json.dumps(message))
def test_stop(ws):
    print(f"Testing stop ")
    message = {
        "type": "stop",
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
