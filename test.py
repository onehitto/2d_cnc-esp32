import websocket
import json

ws_url = "ws://192.168.1.2:4398/ws_hello"  # Replace with your server's URL
#ws_url = "ws://192.168.4.1:4397/ws_setcreadentials"  # Replace with your server's URL
data = {"ssid": "ssid", "password": "password"}

# Function to log and send data
def send_data(ws, data, opcode=websocket.ABNF.OPCODE_TEXT):
    frame_type = {
        websocket.ABNF.OPCODE_CONT: "Continuation Frame",
        websocket.ABNF.OPCODE_TEXT: "Text Frame",
        websocket.ABNF.OPCODE_BINARY: "Binary Frame",
        websocket.ABNF.OPCODE_CLOSE: "Close Frame",
        websocket.ABNF.OPCODE_PING: "Ping Frame",
        websocket.ABNF.OPCODE_PONG: "Pong Frame"
    }.get(opcode, "Unknown Frame Type")
    print(f"Sending WebSocket frame: {frame_type} (Opcode: {opcode})")
    ws.send(data, opcode=opcode)

# Callback when connection is opened
def on_open(ws):
    print("Connection opened")
    try:
        send_data(ws, json.dumps(data))  # Send JSON as a text frame
    except Exception as e:
        print(f"Error while sending data: {e}")

# Callback when a message is received
def on_message(ws, message):
    print(f"Received message: {message}")
    print("Closing connection...")
    ws.close()

# Callback when an error occurs
def on_error(ws, error):
    print(f"WebSocket Error: {error}")

# Callback when the connection is closed
def on_close(ws, close_status_code, close_msg):
    print(f"WebSocket closed: {close_status_code}, {close_msg}")

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