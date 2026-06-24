/*
  ESP32 Advanced CSI + Live Viewing Firmware
  for WiFi Spatial Intelligence System + Aurora Swarm BTC

  Features:
  - CSI capture with detailed info
  - UDP streaming to central pipeline
  - Built-in Web Server for LIVE VIEWING (http://<esp-ip>/)
  - Serial debug output
  - Configurable via defines

  Flash with Arduino IDE (ESP32 board package)
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

// ==================== CONFIGURATION ====================
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

const char* target_ip = "192.168.1.100";   // IP of Python pipeline
const int target_port = 4210;
const char* node_id = "esp32_mining_hall_01";

const int web_port = 80;
// =======================================================

WiFiUDP udp;
WebServer server(web_port);

String last_csi_info = "No data yet";
int packet_count = 0;

// Enhanced CSI callback
void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;

  packet_count++;

  // Build rich JSON payload for pipeline
  String payload = "{";
  payload += "\"node\":\"" + String(node_id) + "\",";
  payload += "\"rssi\":" + String(info->rx_ctrl.rssi) + ",";
  payload += "\"channel\":" + String(info->rx_ctrl.channel) + ",";
  payload += "\"timestamp\":" + String(millis()) + ",";
  payload += "\"csi_len\":" + String(info->len) + ",";
  payload += "\"packet_count\":" + String(packet_count);
  payload += "}";

  // Send to central pipeline
  udp.beginPacket(target_ip, target_port);
  udp.print(payload);
  udp.endPacket();

  // Update live view data
  last_csi_info = "RSSI: " + String(info->rx_ctrl.rssi) + 
                  " | Ch: " + String(info->rx_ctrl.channel) + 
                  " | Len: " + String(info->len) + 
                  " | Packets: " + String(packet_count);
}

// Simple live web page
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta http-equiv='refresh' content='2'>"; // auto refresh
  html += "<title>ESP32 CSI Live View - " + String(node_id) + "</title>";
  html += "<style>body{font-family:monospace;background:#111;color:#0f0;padding:20px;}</style>";
  html += "</head><body>";
  html += "<h1>ESP32 CSI Live View</h1>";
  html += "<h2>Node: " + String(node_id) + "</h2>";
  html += "<p><b>Status:</b> " + last_csi_info + "</p>";
  html += "<p><b>IP:</b> " + WiFi.localIP().toString() + "</p>";
  html += "<p><b>Packets sent:</b> " + String(packet_count) + "</p>";
  html += "<hr><p>Streaming CSI data to central pipeline via UDP...</p>";
  html += "<p>Pipeline: " + String(target_ip) + ":" + String(target_port) + "</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 CSI + Live View Starting ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start UDP
  udp.begin(4210);

  // Enable CSI
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);

  wifi_csi_config_t csi_config = {
    .lltf_en = true,
    .htltf_en = true,
    .stbc_htltf2_en = true,
    .ltf_merge_en = true,
    .channel_filter_en = true,
    .manu_scale = false,
    .shift = 0
  };
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi(true);

  // Web server for live viewing
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started at http://" + WiFi.localIP().toString());
  Serial.println("CSI streaming active. Open the web page for live view.");
}

void loop() {
  server.handleClient();
  // Optional: print status every 5 seconds
  static unsigned long last_print = 0;
  if (millis() - last_print > 5000) {
    Serial.println("[ESP32] " + last_csi_info);
    last_print = millis();
  }
  delay(10);
}