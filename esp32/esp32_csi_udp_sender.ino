/*
  ESP32 CSI + Professional Live Viewing Firmware
  WiFi Spatial Intelligence System v1.1.0

  This firmware turns an ESP32 into a rock-solid CSI sensing node
  with a beautiful, real-time web dashboard.

  Features:
  - Reliable CSI capture
  - UDP streaming to central pipeline
  - Beautiful auto-updating web dashboard
  - WiFi auto-reconnect
  - Clear Serial logging
  - Easy configuration

  Flash with Arduino IDE (ESP32 board)
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

// ================== USER CONFIGURATION ==================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* target_ip = "192.168.1.100";   // IP of your Python pipeline
int target_port = 4210;
const char* node_id = "esp32_mining_hall_01";
// =======================================================

WiFiUDP udp;
WebServer server(80);

// Runtime state
String last_status = "Starting...";
int packet_count = 0;
int rssi = 0;
int channel = 0;
unsigned long last_packet_time = 0;
int packets_per_second = 0;
unsigned long last_pps_calc = 0;
int pps_counter = 0;

// ================== CSI CALLBACK ==================
void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;

  packet_count++;
  pps_counter++;
  rssi = info->rx_ctrl.rssi;
  channel = info->rx_ctrl.channel;
  last_packet_time = millis();

  // Build clean JSON for pipeline
  String payload = "{";
  payload += "\"node\":\"" + String(node_id) + "\",";
  payload += "\"rssi\":" + String(rssi) + ",";
  payload += "\"channel\":" + String(channel) + ",";
  payload += "\"timestamp\":" + String(millis()) + ",";
  payload += "\"csi_len\":" + String(info->len) + ",";
  payload += "\"pps\":" + String(packets_per_second);
  payload += "}";

  udp.beginPacket(target_ip, target_port);
  udp.print(payload);
  udp.endPacket();

  last_status = "Streaming | RSSI: " + String(rssi) + " dBm | Ch: " + String(channel);
}

// ================== WEB DASHBOARD ==================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 CSI Live View</title>
  <style>
    body { font-family: system-ui, -apple-system, sans-serif; background: #0f172a; color: #e2e8f0; margin: 0; padding: 20px; }
    .container { max-width: 800px; margin: 0 auto; }
    h1 { color: #60a5fa; margin-bottom: 8px; }
    .card { background: #1e2937; border-radius: 12px; padding: 24px; margin-bottom: 20px; box-shadow: 0 10px 15px -3px rgb(0 0 0 / 0.1); }
    .metric { display: flex; justify-content: space-between; align-items: center; padding: 12px 0; border-bottom: 1px solid #334155; }
    .metric:last-child { border-bottom: none; }
    .label { color: #94a3b8; font-size: 0.95rem; }
    .value { font-size: 1.35rem; font-weight: 600; color: #60a5fa; }
    .status { padding: 8px 16px; border-radius: 9999px; font-size: 0.9rem; display: inline-block; }
    .status-good { background: #166534; color: #86efac; }
    .status-warn { background: #854d0e; color: #fde047; }
    .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 16px; }
    .footer { text-align: center; color: #64748b; font-size: 0.85rem; margin-top: 30px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 CSI Live View</h1>
    <p style="color:#64748b; margin-top:0;">Node: )rawliteral" + String(node_id) + R"rawliteral(</p>

    <div class="card">
      <div class="metric">
        <span class="label">Status</span>
        <span class="status status-good" id="status">)rawliteral" + last_status + R"rawliteral(</span>
      </div>
      <div class="metric">
        <span class="label">RSSI</span>
        <span class="value" id="rssi">)rawliteral" + String(rssi) + R"rawliteral( dBm</span>
      </div>
      <div class="metric">
        <span class="label">Channel</span>
        <span class="value" id="channel">)rawliteral" + String(channel) + R"rawliteral(</span>
      </div>
      <div class="metric">
        <span class="label">Packets / sec</span>
        <span class="value" id="pps">)rawliteral" + String(packets_per_second) + R"rawliteral(</span>
      </div>
      <div class="metric">
        <span class="label">Total Packets</span>
        <span class="value" id="total">)rawliteral" + String(packet_count) + R"rawliteral(</span>
      </div>
    </div>

    <div class="card">
      <div class="metric">
        <span class="label">Pipeline Target</span>
        <span class="value" style="font-size:1rem;">)rawliteral" + String(target_ip) + ":" + String(target_port) + R"rawliteral(</span>
      </div>
      <div class="metric">
        <span class="label">IP Address</span>
        <span class="value" style="font-size:1rem;">)rawliteral" + WiFi.localIP().toString() + R"rawliteral(</span>
      </div>
    </div>

    <div class="footer">
      WiFi CSI Spatial Intelligence System • Auto-updates every 2s
    </div>
  </div>

  <script>
    setTimeout(() => location.reload(), 2000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n===================================");
  Serial.println("  ESP32 CSI Live View Node");
  Serial.println("  WiFi Spatial Intelligence v1.1.0");
  Serial.println("===================================\n");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connection failed!");
  }

  udp.begin(4210);

  // Enable CSI
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);

  wifi_csi_config_t csi_config = {
    .lltf_en = true, .htltf_en = true, .stbc_htltf2_en = true,
    .ltf_merge_en = true, .channel_filter_en = true,
    .manu_scale = false, .shift = 0
  };
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi(true);

  server.on("/", handleRoot);
  server.begin();

  Serial.println("[Web] Dashboard ready at http://" + WiFi.localIP().toString());
  Serial.println("[CSI] Streaming started.\n");
  last_status = "Connected & Streaming";
}

// ================== LOOP ==================
void loop() {
  server.handleClient();

  // Calculate packets per second
  unsigned long now = millis();
  if (now - last_pps_calc > 1000) {
    packets_per_second = pps_counter;
    pps_counter = 0;
    last_pps_calc = now;
  }

  // WiFi auto-reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Reconnecting...");
    WiFi.reconnect();
    delay(3000);
  }

  // Periodic status on Serial
  static unsigned long last_serial = 0;
  if (now - last_serial > 4000) {
    Serial.printf("[Status] %s | RSSI: %d dBm | PPS: %d | Total: %d\n",
                  last_status.c_str(), rssi, packets_per_second, packet_count);
    last_serial = now;
  }

  delay(10);
}