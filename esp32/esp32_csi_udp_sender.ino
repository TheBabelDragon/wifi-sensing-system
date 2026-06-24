/*
  ESP32 CSI Node - Modular Firmware (Hybrid Ready)

  Current: WiFi CSI + UDP + Web Dashboard
  Future: Modular transport layer (WiFi UDP / LoRa / Meshtastic bridge / Ethernet)

  This firmware is being evolved to support hybrid deployments including:
  - Standard WiFi CSI nodes
  - LoRa / Meshtastic long-range nodes
  - PoE-powered nodes

  Stage 1: Modular structure + documentation
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"

// ================== CONFIGURATION ==================
String node_id = "esp32_mining_hall_01";
String target_ip = "192.168.1.100";
int target_port = 4210;

const char* wifi_ssid = "YOUR_WIFI_SSID";
const char* wifi_password = "YOUR_WIFI_PASSWORD";

const int STATUS_LED = 2;
// ===================================================

WiFiUDP udp;
WebServer server(80);
Preferences preferences;

int packet_count = 0;
int rssi = 0;
int channel = 0;
int packets_per_second = 0;
String last_status = "Booting...";

// ================== TRANSPORT LAYER (Future) ==================
// Currently only WiFi UDP is implemented.
// Future: Add LoRa transport, Meshtastic bridge, Ethernet, etc.
// void send_via_lora(String payload);
// void send_via_meshtastic(String payload);
// ===================================================

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

  // Current transport: WiFi UDP
  udp.beginPacket(target_ip.c_str(), target_port);
  udp.print(payload);
  udp.endPacket();

  // Future: Add conditional transport selection here
  // if (use_lora) send_via_lora(payload);
}

// Web dashboard and rest of code remains similar...
// (Full file continues with existing hardened code)

void setup() {
  Serial.begin(115200);
  delay(600);
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("\n=== ESP32 CSI Hybrid Node (Stage 1) ===");

  preferences.begin("csi-node", false);
  if (preferences.isKey("node_id")) node_id = preferences.getString("node_id", node_id);
  if (preferences.isKey("target_ip")) target_ip = preferences.getString("target_ip", target_ip);
  preferences.end();

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
  }

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

  server.on("/", []() { /* existing dashboard code */ });
  server.begin();

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  Serial.println("[System] Node ready (Hybrid Stage 1)");
  last_status = "Running - Streaming CSI";
  digitalWrite(STATUS_LED, HIGH);
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();

  // Future: Add transport selection / LoRa handling here

  static unsigned long last_log = 0;
  if (millis() - last_log > 8000) {
    Serial.printf("[Status] %s | RSSI:%d | PPS:%d\n", last_status.c_str(), rssi, packets_per_second);
    last_log = millis();
  }

  delay(8);
}