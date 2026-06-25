"""
Meshtastic Gateway Bridge - Improved Example

Focus: Clean integration of pre-assembled Meshtastic devices (W10, etc.)

Key principles:
- Receive from Meshtastic mesh (MQTT is easiest)
- Parse only useful summarized data
- Forward to central system in a simple format

Recommended message format from Meshtastic nodes:
{
  "type": "occupancy" | "event" | "alert" | "heartbeat",
  "node_id": "meshtastic_node_01",
  "timestamp": <unix time>,
  "data": { ... }   // depends on type
}
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
            print("[Gateway] Running in placeholder mode (no paho-mqtt)")
            return

        self.client = mqtt.Client()
        self.client.on_message = self.on_mqtt_message

        try:
            self.client.connect(self.mqtt_broker, self.mqtt_port, 60)
            # Common Meshtastic MQTT topic pattern
            self.client.subscribe("msh/+/json/#")
            self.client.loop_start()
            print(f"[Gateway] Connected to MQTT at {self.mqtt_broker}")
        except Exception as e:
            print(f"[Gateway] MQTT connection error: {e}")

    def on_mqtt_message(self, client, userdata, msg):
        try:
            payload = msg.payload.decode()
            data = json.loads(payload)
            self.process_message(data)
        except Exception as e:
            print(f"[Gateway] Parse error: {e}")

    def process_message(self, data):
        """Process and forward useful data from Meshtastic."""
        msg_type = data.get("type", "unknown")

        if msg_type in ["occupancy", "event", "alert", "heartbeat"]:
            print(f"[Gateway] Useful message: {data}")
            self.forward_to_central(data)
        else:
            # Ignore noise
            pass

    def forward_to_central(self, data):
        """Send to central CSI system."""
        print(f"[Gateway] Forwarding: {data}")
        # TODO: Send via UDP, MQTT topic, or HTTP

    def run(self):
        print("[Gateway] Starting...")
        self.connect_mqtt()

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("[Gateway] Shutting down")

if __name__ == "__main__":
    gateway = MeshtasticGateway()
    gateway.run()