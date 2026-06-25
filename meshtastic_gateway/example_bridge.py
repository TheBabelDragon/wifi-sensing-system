"""
Meshtastic Gateway Bridge - Refined Example

Purpose: Bridge data from pre-assembled Meshtastic devices into the central system.

Features:
- MQTT support (most common with Meshtastic)
- Message type filtering
- Clean structure and error handling
- Ready to be extended

Recommended: Only forward summarized/useful data (events, occupancy, alerts, heartbeats).
"""

import json
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    mqtt = None

class MeshtasticGateway:
    def __init__(self, mqtt_broker="localhost", mqtt_port=1883):
        self.mqtt_broker = mqtt_broker
        self.mqtt_port = mqtt_port
        self.client = None

    def connect_mqtt(self):
        if mqtt is None:
            print("[Gateway] paho-mqtt not available - running in simulation mode")
            return False

        self.client = mqtt.Client()
        self.client.on_message = self.on_mqtt_message

        try:
            self.client.connect(self.mqtt_broker, self.mqtt_port, 60)
            self.client.subscribe("msh/+/json/#")  # Common Meshtastic topic pattern
            self.client.loop_start()
            print(f"[Gateway] Connected to MQTT broker at {self.mqtt_broker}")
            return True
        except Exception as e:
            print(f"[Gateway] MQTT connection failed: {e}")
            return False

    def on_mqtt_message(self, client, userdata, msg):
        try:
            payload = msg.payload.decode()
            data = json.loads(payload)
            self.process_message(data)
        except Exception as e:
            print(f"[Gateway] Failed to parse message: {e}")

    def process_message(self, data):
        msg_type = data.get("type", "unknown")

        # Only forward useful message types
        if msg_type in ["occupancy", "event", "alert", "heartbeat"]:
            print(f"[Gateway] Useful message received: {msg_type}")
            self.forward_to_central(data)
        else:
            pass  # Ignore other messages

    def forward_to_central(self, data):
        print(f"[Gateway] Forwarding to central system: {data}")
        # TODO: Implement actual forwarding (UDP / MQTT / HTTP)

    def run(self):
        print("[Gateway] Starting...")
        connected = self.connect_mqtt()

        try:
            while True:
                if not connected:
                    # Simulation mode
                    time.sleep(5)
                    print("[Gateway] (Simulation) No Meshtastic connection")
                else:
                    time.sleep(1)
        except KeyboardInterrupt:
            print("[Gateway] Shutting down...")

if __name__ == "__main__":
    gateway = MeshtasticGateway()
    gateway.run()