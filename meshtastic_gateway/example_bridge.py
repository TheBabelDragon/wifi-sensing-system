"""
Meshtastic Gateway Bridge Example

This example shows a practical structure for bridging data from
pre-assembled Meshtastic devices into the central system.

Common connection methods:
- MQTT (most common with Meshtastic)
- Serial
- Meshtastic API

This version includes a commented MQTT example.
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
        """Connect to Meshtastic MQTT broker (if available)."""
        if mqtt is None:
            print("[Gateway] paho-mqtt not installed. Using placeholder mode.")
            return

        self.client = mqtt.Client()
        self.client.on_message = self.on_mqtt_message

        try:
            self.client.connect(self.mqtt_broker, self.mqtt_port, 60)
            # Subscribe to Meshtastic topics (adjust as needed)
            self.client.subscribe("msh/+/json/#")
            self.client.loop_start()
            print(f"[Gateway] Connected to MQTT broker at {self.mqtt_broker}")
        except Exception as e:
            print(f"[Gateway] MQTT connection failed: {e}")

    def on_mqtt_message(self, client, userdata, msg):
        """Handle incoming Meshtastic message."""
        try:
            payload = msg.payload.decode()
            data = json.loads(payload)
            self.process_meshtastic_data(data)
        except Exception as e:
            print(f"[Gateway] Failed to parse message: {e}")

    def process_meshtastic_data(self, data):
        """Process and forward relevant data from Meshtastic."""
        # Example: Extract useful fields and forward
        print(f"[Gateway] Received from Meshtastic: {data}")

        # Forward to central system (example)
        self.forward_to_central(data)

    def forward_to_central(self, data):
        """Send processed data to the main CSI system."""
        print(f"[Gateway] Forwarding to central system: {data}")
        # TODO: Send via UDP, MQTT, or HTTP to the main pipeline

    def run(self):
        print("[Gateway] Starting...")
        self.connect_mqtt()

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("[Gateway] Shutting down...")

if __name__ == "__main__":
    gateway = MeshtasticGateway()
    gateway.run()