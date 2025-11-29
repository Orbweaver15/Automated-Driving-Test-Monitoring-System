from flask import Flask, render_template
import serial
import serial.tools.list_ports
import time
import threading
from flask_socketio import SocketIO, emit
import logging

app = Flask(__name__)
socketio = SocketIO(app, async_mode='threading', max_http_buffer_size=1e8)

# Global variables
serial_connection = None
reading_active = True
arduino_thread = None
game_active = False
sensor_data = "00,0.00"
serial_lock = threading.Lock()

# Command mapping
command_map = {
    '1': '1',
    '2': '2',
    '3': '3',
    'start': 'G',
    'stop': 'X'
}

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def find_arduino_port():
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if any(x in port.description.lower() for x in ['arduino', 'ch340', 'cp210', 'ftdi', 'usb serial']):
            return port.device
    return None

def connect_to_arduino():
    global serial_connection
    port = find_arduino_port()
    if not port:
        return False
    
    try:
        serial_connection = serial.Serial(port=port, baudrate=9600, timeout=1)
        time.sleep(2)  # Wait for connection to stabilize
        return True
    except Exception as e:
        logger.error(f"Connection error: {e}")
        return False

def read_from_arduino():
    global serial_connection, reading_active
    buffer = ""
    
    while reading_active:
        if serial_connection and serial_connection.is_open:
            try:
                with serial_lock:
                    # Read all available data
                    raw_data = serial_connection.read(serial_connection.in_waiting or 1)
                    if raw_data:
                        buffer += raw_data.decode('utf-8', errors='ignore')
                        
                        # Process complete lines
                        while '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                            line = line.strip()
                            if line:
                                process_serial_line(line)
            except Exception as e:
                logger.error(f"Serial read error: {e}")
                buffer = ""
                time.sleep(0.1)
        else:
            time.sleep(1)

            
def process_serial_line(line):
    global sensor_data, game_active
    
    if len(line) >= 5 and ',' in line:
        sensor_data = line
        socketio.emit('sensor_data', line)
    elif "GAME_START" in line:
        game_active = True
        socketio.emit('game_state', {'game_active': True, 'score': 0})
    else:
        socketio.emit('console_output', line)

@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def handle_connect():
    logger.info('Client connected')
    status = serial_connection and serial_connection.is_open
    emit('connection_status', {'connected': status})

@socketio.on('command')
def handle_command(data):
    cmd_type = data['type']
    cmd = data['command']
    arduino_cmd = command_map.get(cmd, cmd)
    
    if cmd_type == 'test':
        logger.info(f"Sending test command: {arduino_cmd}")
        send_arduino_command(arduino_cmd)
    elif cmd_type == 'game':
        if cmd == 'start':
            logger.info("Starting game")
            send_arduino_command('G')
        elif cmd == 'stop':
            logger.info("Stopping game")
            global game_active
            game_active = False
            send_arduino_command('X')

def send_arduino_command(command):
    global serial_connection
    with serial_lock:
        if serial_connection and serial_connection.is_open:
            try:
                # Clear buffers
                serial_connection.reset_input_buffer()
                serial_connection.reset_output_buffer()
                time.sleep(0.1)
                
                # Send command with proper line endings
                serial_connection.write((command + '\r\n').encode('utf-8'))
                serial_connection.flush()
                time.sleep(0.2)  # Give Arduino time to process
                return True
            except Exception as e:
                logger.error(f"Command send error: {e}")
                try:
                    serial_connection.close()
                except:
                    pass
        return False

if __name__ == '__main__':
    if connect_to_arduino():
        logger.info("Connected to Arduino")
    else:
        logger.warning("Could not find Arduino. Retrying in background...")
    
    reading_active = True
    arduino_thread = threading.Thread(target=read_from_arduino, daemon=True)
    arduino_thread.start()
    
    logger.info("Starting server...")
    socketio.run(app, debug=False, port=5000, allow_unsafe_werkzeug=True)