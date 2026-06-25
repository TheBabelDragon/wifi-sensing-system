"""
Meshtastic Gateway Bridge - Final Refined Version

This is a clean, well-structured starting point for bridging
pre-assembled Meshtastic devices into the central CSI system.

Key design choices:
- Only forward useful summarized data
- MQTT is the recommended connection method
- Clear separation between receiving, processing, and forwarding
- Easy to extend with real forwarding logic
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
            print("[Gateway] Running without paho-mqtt (simulation mode)")
            return False

        self.client = mqtt.Client()
        self.client.on_message = self.on_mqtt_message

        try:
            self.client.connect(self.mqtt_broker, self.mqtt_port, 60)
            self.client.subscribe("msh/+/json/#")
            self.client.loop_start()
            print(f"[Gateway] Connected to MQTT at {self.mqtt_broker}")
            return True
        except Exception as e:
            print(f"[Gateway] Connection failed: {e}")
            return False

    def on_mqtt_message(self, client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode())
            self.process_message(data)
        except Exception as e:
            print(f"[Gateway] Parse error: {e}")

    def process_message(self, data):
        msg_type = data.get("type", "unknown")

        if msg_type in ["occupancy", "event", "alert", "heartbeat"]:
            print(f"[Gateway] Processing {msg_type}")
            self.forward_to_central(data)

    def forward_to_central(self, data):
        print(f"[Gateway] Forwarding: {data}")
        # TODO: Implement actual transport (UDP/MQTT/HTTP)

    def run(self):
        print("[Gateway] Starting...")
        connected = self.connect_mqtt()

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("[Gateway] Shutting down")

if __name__ == "__main__":
    gateway = MeshtasticGateway()
    gateway.run()