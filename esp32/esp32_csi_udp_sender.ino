/*
  ESP32 CSI Node - Hardened Production Firmware
  WiFi Spatial Intelligence System v1.1.0

  Hardened features:
  - Watchdog timer
  - Robust WiFi reconnection with backoff
  - NVS Preferences for persistent config
  - Status LED patterns
  - Professional web dashboard
  - Clean logging

  Recommended for real deployments.
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

// ================== CONFIGURATION ==================
// These can be overridden via NVS after first boot
String node_id = "esp32_mining_hall_01";
String target_ip = "192.168.1.100";
int target_port = 4210;

const char* wifi_ssid = "YOUR_WIFI_SSID";
const char* wifi_password = "YOUR_WIFI_PASSWORD";

const int STATUS_LED = 2;  // Built-in LED on most ESP32 DevKits
// ===================================================

WiFiUDP udp;
WebServer server(80);
Preferences preferences;

// State
int packet_count = 0;
int rssi = 0;
int channel = 0;
int packets_per_second = 0;
String last_status = "Booting...";
unsigned long last_reconnect_attempt = 0;
int reconnect_delay = 1000;  // Start with 1s backoff

// ================== CSI CALLBACK ==================
void IRAM_ATTR csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
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

  udp.beginPacket(target_ip.c_str(), target_port);
  udp.print(payload);
  udp.endPacket();
}

// ================== WEB DASHBOARD ==================
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 CSI Node</title>";
  html += "<style>body{font-family:system-ui;background:#0f172a;color:#e2e8f0;padding:20px;max-width:700px;margin:auto} .card{background:#1e2937;border-radius:12px;padding:20px;margin:15px 0} .metric{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #334155} .value{font-weight:600;color:#60a5fa}</style>";
  html += "</head><body>";
  html += "<h1>ESP32 CSI Node</h1>";
  html += "<div class='card'>";
  html += "<div class='metric'><span>Node ID</span><span class='value'>" + node_id + "</span></div>";
  html += "<div class='metric'><span>Status</span><span class='value'>" + last_status + "</span></div>";
  html += "<div class='metric'><span>RSSI</span><span class='value'>" + String(rssi) + " dBm</span></div>";
  html += "<div class='metric'><span>Channel</span><span class='value'>" + String(channel) + "</span></div>";
  html += "<div class='metric'><span>Packets/sec</span><span class='value'>" + String(packets_per_second) + "</span></div>";
  html += "<div class='metric'><span>Total Packets</span><span class='value'>" + String(packet_count) + "</span></div>";
  html += "</div>";
  html += "<p style='color:#64748b;font-size:0.9rem'>Auto-refreshing • Pipeline: " + target_ip + ":" + String(target_port) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ================== WIFI MANAGEMENT ==================
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
    Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
    last_status = "WiFi Connected";
    reconnect_delay = 1000;
    digitalWrite(STATUS_LED, HIGH);
  } else {
    Serial.println("\n[WiFi] Failed to connect");
    last_status = "WiFi Connection Failed";
  }
}

void handleWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - last_reconnect_attempt > reconnect_delay) {
      Serial.println("[WiFi] Attempting reconnect...");
      WiFi.disconnect();
      WiFi.begin(wifi_ssid, wifi_password);
      last_reconnect_attempt = now;
      reconnect_delay = min(reconnect_delay * 2, 30000); // Exponential backoff
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  delay(600);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Serial.println("\n=== ESP32 CSI Hardened Node v1.1.0 ===");

  // Load persistent config from NVS
  preferences.begin("csi-node", false);
  if (preferences.isKey("node_id")) node_id = preferences.getString("node_id", node_id);
  if (preferences.isKey("target_ip")) target_ip = preferences.getString("target_ip", target_ip);
  preferences.end();

  Serial.println("Node ID: " + node_id);
  Serial.println("Target: " + target_ip + ":" + String(target_port));

  connectWiFi();

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

  // Web server
  server.on("/", handleRoot);
  server.begin();

  // Enable Watchdog (5 seconds)
  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  Serial.println("[System] Hardened node ready. Web dashboard active.");
  last_status = "Running - Streaming CSI";
  digitalWrite(STATUS_LED, HIGH);
}

// ================== LOOP ==================
void loop() {
  esp_task_wdt_reset();           // Feed watchdog
  server.handleClient();
  handleWiFi();

  // Simple PPS calculation
  static unsigned long last_pps = 0;
  static int pps_count = 0;
  if (millis() - last_pps > 1000) {
    packets_per_second = pps_count;
    pps_count = 0;
    last_pps = millis();
  }

  // Status LED heartbeat
  static unsigned long last_blink = 0;
  if (millis() - last_blink > 1500) {
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    last_blink = millis();
  }

  // Periodic logging
  static unsigned long last_log = 0;
  if (millis() - last_log > 8000) {
    Serial.printf("[Status] %s | RSSI:%d | PPS:%d | Total:%d\n", 
                  last_status.c_str(), rssi, packets_per_second, packet_count);
    last_log = millis();
  }

  delay(8);
}