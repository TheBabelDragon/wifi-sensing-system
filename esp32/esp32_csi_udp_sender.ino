/*
  ESP32 CSI Node - Well-Refined Hybrid Firmware

  Transports (in order of preference for low interference):
  1. ESP-NOW (local, connectionless, good for mesh)
  2. LoRa / Meshtastic (long range)
  3. WiFi UDP (only when internet/gateway reachability is needed)

  ESP-NOW now supports basic command receiving.
*/

#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_now.h>
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

// ================== TRANSPORT FLAGS ==================
bool use_wifi = false;
bool use_esp_now = true;
bool use_lora = false;
bool use_meshtastic_bridge = false;

uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optional: log send status
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  String msg = "";
  for (int i = 0; i < len; i++) msg += (char)incomingData[i];

  Serial.printf("[ESP-NOW] From %02X:%02X:%02X:%02X:%02X:%02X → %s\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], msg.c_str());

  // Basic command handling example
  if (msg.startsWith("LED_ON")) {
    digitalWrite(STATUS_LED, HIGH);
  } else if (msg.startsWith("LED_OFF")) {
    digitalWrite(STATUS_LED, LOW);
  } else if (msg.startsWith("STATUS")) {
    Serial.printf("[Node] Packets: %d | RSSI: %d\n", packet_count, rssi);
  }
}

void send_via_esp_now(String payload) {
  uint8_t data[250];
  payload.getBytes(data, sizeof(data));
  esp_now_send(broadcastAddress, data, payload.length());
}

// ================== OTHER TRANSPORTS ==================
void send_via_lora(String payload) {
  Serial.println("[LoRa] Would send: " + payload);
}

void send_via_meshtastic(String payload) {
  Serial.println("[Meshtastic] Would forward: " + payload);
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

void setup() {
  Serial.begin(115200);
  delay(600);
  pinMode(STATUS_LED, OUTPUT);

  Serial.println("\n=== ESP32 CSI Hybrid Node (ESP-NOW + Optional WiFi) ===");

  preferences.begin("csi-node", false);
  if (preferences.isKey("node_id")) node_id = preferences.getString("node_id", node_id);
  if (preferences.isKey("target_ip")) target_ip = preferences.getString("target_ip", target_ip);
  preferences.end();

  WiFi.mode(WIFI_STA);

  if (use_wifi) {
    WiFi.begin(wifi_ssid, wifi_password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(400);
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
    }
    Serial.println("[WiFi] Connected");
    udp.begin(4210);
  }

  if (use_esp_now) {
    if (esp_now_init() != ESP_OK) {
      Serial.println("[ESP-NOW] Init failed");
    } else {
      esp_now_register_send_cb(OnDataSent);
      esp_now_register_recv_cb(OnDataRecv);

      esp_now_peer_info_t peerInfo;
      memset(&peerInfo, 0, sizeof(peerInfo));
      memcpy(peerInfo.peer_addr, broadcastAddress, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;

      esp_now_add_peer(&peerInfo);
      Serial.println("[ESP-NOW] Ready (broadcast)");
    }
  }

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);

  wifi_csi_config_t csi_config = {
    .lltf_en = true, .htltf_en = true, .stbc_htltf2_en = true,
    .ltf_merge_en = true, .channel_filter_en = true,
    .manu_scale = false, .shift = 0
  };
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi(true);

  if (use_wifi) {
    server.on("/", []() { /* dashboard */ });
    server.begin();
  }

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  Serial.println("[System] Node ready");
  last_status = use_esp_now ? "ESP-NOW Mode" : "Passive CSI";
  digitalWrite(STATUS_LED, HIGH);
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