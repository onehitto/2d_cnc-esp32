import websocket
import json

ws_url = "ws://192.168.1.2:8080/ws"  # Replace with your server's URL
data = {"key": "value", "action": "send"}

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

def on_open(ws):
    print("Connection opened")
    try:
        send_data(ws, json.dumps(data))  # Send JSON as a text frame
    except Exception as e:
        print(f"Error while sending data: {e}")
    finally:
        ws.close()

def on_error(ws, error):
    print(f"WebSocket Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print(f"WebSocket closed: {close_status_code}, {close_msg}")

ws = websocket.WebSocketApp(
    ws_url,
    on_open=on_open,
    on_error=on_error,
    on_close=on_close,
)
ws.run_forever()