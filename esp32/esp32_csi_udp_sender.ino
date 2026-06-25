/*
  ESP32 CSI Node - Hybrid / Low-Interference Firmware (Redirected)

  Design Goals (Updated):
  - CSI sensing should be as passive/non-interfering as possible
  - WiFi association should be OPTIONAL, not mandatory
  - Only join WiFi when actually needed (gateway reachability or full swarm)
  - Prefer lower-interference transports when available (ESP-NOW, LoRa/Meshtastic)

  Transport Priority (configurable):
  1. LoRa / Meshtastic (long range, low interference)
  2. ESP-NOW (local, connectionless)
  3. WiFi UDP (only when needed for reachability)
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

// ================== TRANSPORT CONTROL ==================
// Set these according to your deployment needs
bool use_wifi = false;              // Only enable if you need to reach a gateway/internet
bool use_esp_now = false;           // Local low-interference option
bool use_lora = false;              // Long-range (pre-assembled Meshtastic recommended)
bool use_meshtastic_bridge = false;

// ================== LORA / MESHTASTIC PLACEHOLDERS ==================
void send_via_lora(String payload) {
  Serial.println("[LoRa] Would send: " + payload);
}

void send_via_meshtastic(String payload) {
  Serial.println("[Meshtastic] Would forward: " + payload);
}

void send_via_esp_now(String payload) {
  Serial.println("[ESP-NOW] Would send: " + payload);
  // Future: esp_now_send(...)
}
// =======================================================

WiFiUDP udp;
WebServer server(80);
Preferences preferences;

int packet_count = 0;
int rssi = 0;
int channel = 0;
String last_status = "Booting...";

void send_payload(String payload) {
  if (use_lora) {
    send_via_lora(payload);
  } else if (use_meshtastic_bridge) {
    send_via_meshtastic(payload);
  } else if (use_esp_now) {
    send_via_esp_now(payload);
  } else if (use_wifi) {
    udp.beginPacket(target_ip.c_str(), target_port);
    udp.print(payload);
    udp.endPacket();
  } else {
    // No transport enabled - just log locally
    Serial.println("[Node] No transport enabled. Payload: " + payload);
  }
}

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

// Web dashboard only starts if WiFi is enabled
void startWebServerIfNeeded() {
  if (use_wifi) {
    server.on("/", []() { /* dashboard */ });
    server.begin();
    Serial.println("[Web] Dashboard started (WiFi mode)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(600);
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("\n=== ESP32 CSI Hybrid Node (Low-Interference Mode) ===");

  preferences.begin("csi-node", false);
  if (preferences.isKey("node_id")) node_id = preferences.getString("node_id", node_id);
  if (preferences.isKey("target_ip")) target_ip = preferences.getString("target_ip", target_ip);
  preferences.end();

  // Only connect to WiFi if explicitly enabled
  if (use_wifi) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(400);
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Connected (full connectivity mode)");
      last_status = "WiFi Connected";
      digitalWrite(STATUS_LED, HIGH);
      udp.begin(4210);
    } else {
      Serial.println("[WiFi] Failed to connect");
      last_status = "WiFi Failed";
    }
  } else {
    Serial.println("[WiFi] Skipped (low-interference / passive mode)");
    last_status = "Passive CSI Mode";
    digitalWrite(STATUS_LED, HIGH);
  }

  // Always enable CSI capture (passive)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);

  wifi_csi_config_t csi_config = {
    .lltf_en = true, .htltf_en = true, .stbc_htltf2_en = true,
    .ltf_merge_en = true, .channel_filter_en = true,
    .manu_scale = false, .shift = 0
  };
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi(true);

  startWebServerIfNeeded();

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  Serial.println("[System] Node ready (Low-Interference Mode)");
}

void loop() {
  esp_task_wdt_reset();

  if (use_wifi) {
    server.handleClient();
  }

  static unsigned long last_log = 0;
  if (millis() - last_log > 8000) {
    Serial.printf("[Status] %s | RSSI:%d | Packets:%d\n", last_status.c_str(), rssi, packet_count);
    last_log = millis();
  }

  delay(8);
}