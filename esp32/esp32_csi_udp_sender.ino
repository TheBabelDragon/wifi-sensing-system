/*
  ESP32 CSI Node - Modular Hybrid Firmware (Stage 2 Start)

  Stage 1: Modular structure + documentation (Complete)
  Stage 2: LoRa scaffolding + Meshtastic bridging guidance

  Hardware assumptions for LoRa:
  - ESP32 + SX1262 or SX1276 module
  - Common wiring: NSS, DIO1, RST, BUSY on specific GPIOs
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

// ================== LORA SCAFFOLDING (Stage 2) ==================
// Uncomment and configure when adding actual LoRa support
// #include <RadioLib.h>
// SX1262 radio = new Module(5, 2, 15, 4); // Example pins (NSS, DIO1, RST, BUSY)

bool use_lora = false;           // Set true to enable LoRa transport
bool use_meshtastic_bridge = false;

void send_via_lora(String payload) {
  // Placeholder for future LoRa implementation
  // radio.transmit(payload);
  Serial.println("[LoRa] Would send: " + payload);
}

void send_via_meshtastic(String payload) {
  // Placeholder for Meshtastic bridge
  // Could send to Meshtastic via serial, MQTT, or API
  Serial.println("[Meshtastic] Would forward: " + payload);
}
// ============================================================

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
  } else {
    // Default: WiFi UDP
    udp.beginPacket(target_ip.c_str(), target_port);
    udp.print(payload);
    udp.endPacket();
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

// Web dashboard and setup/loop remain similar to Stage 1...
// (Keeping existing hardened code for WiFi + CSI + Web)

void setup() {
  Serial.begin(115200);
  delay(600);
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("\n=== ESP32 CSI Hybrid Node - Stage 2 ===");

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

  server.on("/", []() { /* existing dashboard */ });
  server.begin();

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  Serial.println("[System] Stage 2 Hybrid Node ready");
  last_status = "Running - Streaming CSI";
  digitalWrite(STATUS_LED, HIGH);
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();

  static unsigned long last_log = 0;
  if (millis() - last_log > 8000) {
    Serial.printf("[Status] %s | RSSI:%d | Packets:%d\n", last_status.c_str(), rssi, packet_count);
    last_log = millis();
  }

  delay(8);
}