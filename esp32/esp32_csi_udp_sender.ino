/*
  ESP32 CSI Node - Modular Firmware v1.1 (Stage 1 Complete)

  Purpose:
  - Capture WiFi CSI
  - Send data over chosen transport
  - Provide local web dashboard

  Stage 1 Focus: Modular structure + clear extension points

  Future stages will add:
  - LoRa transport
  - Meshtastic bridging
  - Multi-backhaul support
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

// ================== CONFIG ==================
String node_id = "esp32_hybrid_01";
String target_ip = "192.168.1.100";
int target_port = 4210;

const char* wifi_ssid = "YOUR_WIFI_SSID";
const char* wifi_password = "YOUR_WIFI_PASSWORD";

const int STATUS_LED = 2;
// ============================================

WiFiUDP udp;
WebServer server(80);
Preferences preferences;

// State
int packet_count = 0;
int rssi = 0;
int channel = 0;
String last_status = "Booting...";

// ================== TRANSPORT LAYER ==================
// Currently only WiFi UDP is active.
// Future: Add support for LoRa, Meshtastic, Ethernet, etc.

void send_payload(String payload) {
  // Default transport: WiFi UDP
  udp.beginPacket(target_ip.c_str(), target_port);
  udp.print(payload);
  udp.endPacket();

  // Future example:
  // if (current_transport == LORA) send_via_lora(payload);
  // if (current_transport == MESHTASTIC) send_via_meshtastic(payload);
}
// ====================================================

void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;

  packet_count++;
  rssi = info->rx_ctrl.rssi;
  channel = info->rx_ctrl.channel;

  String payload = "{";
  payload += "\"node\":\"" + node_id + "\",";
  payload += "\"rssi\":" + String(rssi) + ",";
  payload += "\"channel\":" + String(channel) + ",";
  payload += "\"timestamp\":" + String(millis()) + ",";
  payload += "\"csi_len\":" + String(info->len);
  payload += "}";

  send_payload(payload);
}

// ================== WEB DASHBOARD ==================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 CSI Node</title>";
  html += "<style>body{font-family:system-ui;background:#0f172a;color:#e2e8f0;padding:20px;max-width:700px;margin:auto} .card{background:#1e2937;border-radius:12px;padding:20px;margin:15px 0} .metric{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #334155} .value{font-weight:600;color:#60a5fa}</style>";
  html += "</head><body>";
  html += "<h1>ESP32 CSI Node (Hybrid Stage 1)</h1>";
  html += "<div class='card'>";
  html += "<div class='metric'><span>Node ID</span><span class='value'>" + node_id + "</span></div>";
  html += "<div class='metric'><span>Status</span><span class='value'>" + last_status + "</span></div>";
  html += "<div class='metric'><span>RSSI</span><span class='value'>" + String(rssi) + " dBm</span></div>";
  html += "<div class='metric'><span>Channel</span><span class='value'>" + String(channel) + "</span></div>";
  html += "<div class='metric'><span>Total Packets</span><span class='value'>" + String(packet_count) + "</span></div>";
  html += "</div>";
  html += "<p style='color:#64748b;font-size:0.9rem'>Stage 1 - Modular Hybrid Ready</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ================== WIFI + CSI ==================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting to WiFi");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(400);
    Serial.print(".");
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected");
    last_status = "WiFi Connected";
    digitalWrite(STATUS_LED, HIGH);
  } else {
    Serial.println("\n[WiFi] Failed to connect");
    last_status = "WiFi Connection Failed";
  }
}

void setup() {
  Serial.begin(115200);
  delay(600);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Serial.println("\n=== ESP32 CSI Hybrid Node - Stage 1 ===");

  preferences.begin("csi-node", false);
  if (preferences.isKey("node_id")) node_id = preferences.getString("node_id", node_id);
  if (preferences.isKey("target_ip")) target_ip = preferences.getString("target_ip", target_ip);
  preferences.end();

  connectWiFi();

  udp.begin(4210);

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

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  Serial.println("[System] Stage 1 Hybrid Node ready");
  last_status = "Running - Streaming CSI";
  digitalWrite(STATUS_LED, HIGH);
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();

  // Future: Add transport switching / LoRa handling here

  static unsigned long last_log = 0;
  if (millis() - last_log > 8000) {
    Serial.printf("[Status] %s | RSSI:%d | Packets:%d\n", last_status.c_str(), rssi, packet_count);
    last_log = millis();
  }

  delay(8);
}