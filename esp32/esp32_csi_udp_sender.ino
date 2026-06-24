/*
  ESP32 CSI UDP Sender for WiFi Spatial Intelligence System
  Flashes easily via Arduino IDE

  Sends CSI + RSSI data as JSON over UDP to the central Python pipeline.
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

// === CONFIGURE THESE ===
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* target_ip = "192.168.1.100";   // IP of machine running Python pipeline
const int target_port = 4210;               // UDP port the Python ingestor listens on
const char* node_id = "esp32_mining_hall_01"; // Unique ID for this node
// =======================

WiFiUDP udp;

// Simple CSI callback (basic version - extend with full CSI parsing as needed)
void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;

  // Build JSON payload
  String payload = "{\"node\":\"" + String(node_id) + "\",";
  payload += "\"rssi\":" + String(info->rx_ctrl.rssi) + ",";
  payload += "\"timestamp\":" + String(millis()) + ",";
  payload += "\"csi_len\":" + String(info->len) + "}";

  // Send via UDP
  udp.beginPacket(target_ip, target_port);
  udp.print(payload);
  udp.endPacket();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

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

  udp.begin(4210); // local port
  Serial.println("CSI UDP sender started. Streaming to " + String(target_ip));
}

void loop() {
  // Optionally add periodic status or deep sleep later
  delay(100);
}
