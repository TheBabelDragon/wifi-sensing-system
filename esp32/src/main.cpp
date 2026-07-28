#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <cmath>

// ============================================================
// ESP32 CSI → LAN broadcast :4210 (Echo Grid / wifi-sensing)
// No host IP config required once on the same Wi-Fi.
// ============================================================

const uint16_t TARGET_PORT      = 4210;
const char* NODE_ID             = "esp32_node_01";
const uint32_t SEND_INTERVAL_MS = 450;
const int STATUS_LED_PIN        = 2;

const bool USE_ESP_NOW  = false;  // off by default — was spamming errors
const bool USE_REAL_CSI = true;

float latestRealCSI[32];
float prevCSI[32];
bool hasNewCSI = false;

float bandMovement[4] = {0};
float movementIntensity = 0.0;
float activityLevel = 0;
float nodeConfidence = 0.6;
int hotZoneCount = 0;
bool significantObstruction = false;
float csiVariance = 0;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int packetCount = 0;

void statusLed(bool on) {
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void updateRichCSIFeatures() {
  for (int b = 0; b < 4; b++) {
    float maxChange = 0;
    for (int i = 0; i < 8; i++) {
      int idx = b * 8 + i;
      float change = fabsf(latestRealCSI[idx] - prevCSI[idx]);
      if (change > maxChange) maxChange = change;
    }
    bandMovement[b] = maxChange;
  }

  float meanAll = 0;
  for (int i = 0; i < 32; i++) meanAll += latestRealCSI[i];
  meanAll /= 32.0f;

  float varianceAll = 0;
  for (int i = 0; i < 32; i++) {
    float d = latestRealCSI[i] - meanAll;
    varianceAll += d * d;
  }
  csiVariance = varianceAll / 32.0f;

  float strongest = 0;
  for (int b = 0; b < 4; b++)
    if (bandMovement[b] > strongest) strongest = bandMovement[b];

  movementIntensity = constrain(strongest * 4.8f + csiVariance * 1.6f, 0.0f, 1.0f);
  activityLevel = constrain(csiVariance * 6.2f + movementIntensity * 0.9f, 0.0f, 1.0f);
  hotZoneCount = constrain((int)(activityLevel * 6), 0, 6);
  significantObstruction = (hotZoneCount >= 3) || (activityLevel > 0.58f);
  nodeConfidence = constrain(0.4f + activityLevel * 0.5f, 0.35f, 0.92f);

  for (int i = 0; i < 32; i++) prevCSI[i] = latestRealCSI[i];
}

void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf || info->len < 2) return;
  int numSub = min(32, info->len / 2);
  for (int i = 0; i < numSub; i++) {
    int8_t re = info->buf[i * 2];
    int8_t im = info->buf[i * 2 + 1];
    float amp = sqrtf((float)re * re + (float)im * im) / 200.0f;
    latestRealCSI[i] = amp > 1.0f ? 1.0f : amp;
  }
  for (int i = numSub; i < 32; i++) latestRealCSI[i] = 0.0f;
  hasNewCSI = true;
  updateRichCSIFeatures();
}

bool initRealCSI() {
  if (esp_wifi_set_promiscuous(true) != ESP_OK) return false;

  wifi_csi_config_t cfg = {};
  cfg.lltf_en = true;
  cfg.htltf_en = true;
  cfg.stbc_htltf2_en = true;
  cfg.ltf_merge_en = true;
  cfg.manu_scale = false;

  if (esp_wifi_set_csi_config(&cfg) != ESP_OK) return false;
  if (esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL) != ESP_OK) return false;
  if (esp_wifi_set_csi(true) != ESP_OK) return false;

  for (int i = 0; i < 32; i++) {
    latestRealCSI[i] = 0.3f;
    prevCSI[i] = 0.3f;
  }
  Serial.println("[CSI] hardware OK");
  return true;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);

  String apName = String("ESP32-CSI-") + NODE_ID;
  if (wifiManager.autoConnect(apName.c_str())) {
    wifiConnected = true;
    statusLed(true);
    Serial.print("[WiFi] ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[WiFi] failed");
  }
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float rssi = WiFi.RSSI();

  if (!hasNewCSI) {
    for (int i = 0; i < 32; i++)
      latestRealCSI[i] = 0.35f + (random(30) / 100.0f);
    updateRichCSIFeatures();
  }

  StaticJsonDocument<1600> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)rssi;
  doc["type"] = "wifi_csi";
  doc["activity"] = activityLevel;
  doc["hot_zones"] = hotZoneCount;
  doc["obstruction"] = significantObstruction;
  doc["movement_intensity"] = movementIntensity;
  doc["confidence"] = nodeConfidence;

  JsonArray bandMov = doc.createNestedArray("band_movement");
  for (int b = 0; b < 4; b++) bandMov.add(bandMovement[b]);

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);

  char buf[1600];
  size_t n = serializeJson(doc, buf);

  // Broadcast — any host on the LAN listening :4210 gets it (Echo Grid)
  IPAddress bcast = WiFi.localIP();
  bcast[3] = 255;

  bool ok = false;
  if (udp.beginPacket(bcast, TARGET_PORT)) {
    udp.write((uint8_t*)buf, n);
    ok = udp.endPacket();
  }
  // Also try global broadcast
  if (udp.beginPacket(IPAddress(255, 255, 255, 255), TARGET_PORT)) {
    udp.write((uint8_t*)buf, n);
    udp.endPacket();
  }

  packetCount++;
  hasNewCSI = false;

  Serial.printf("[UDP] bcast:%u ok=%d act=%.2f mov=%.2f rssi=%.0f\n",
                TARGET_PORT, ok ? 1 : 0, activityLevel, movementIntensity, rssi);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("=== ESP32 CSI (broadcast :4210) ===");

  pinMode(STATUS_LED_PIN, OUTPUT);
  statusLed(false);

  WiFi.mode(WIFI_STA);
  delay(50);

  connectWiFi();

  if (USE_REAL_CSI && !initRealCSI()) {
    Serial.println("[CSI] hardware CSI unavailable — soft CSI");
  }

  udp.begin(4211);
  Serial.println("Setup complete — no host IP needed");
}

void loop() {
  if (millis() - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = millis();
  }
  delay(10);
}
