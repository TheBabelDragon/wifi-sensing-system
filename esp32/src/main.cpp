#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <Preferences.h>
#include <cmath>

// Optional RGB — only if board has NeoPixel on GPIO2
#ifndef USE_NEOPIXEL
#define USE_NEOPIXEL 0
#endif

#if USE_NEOPIXEL
#include <Adafruit_NeoPixel.h>
Adafruit_NeoPixel rgbLed(1, 2, NEO_GRB + NEO_KHZ800);
#endif

// ============================================================
// ESP32 CSI Node — stable boot order for standard DevKit
// UDP :4210 → Echo Grid / wifi-sensing host
// ============================================================

const uint16_t TARGET_PORT      = 4210;
const char* NODE_ID             = "esp32_node_01";
const uint32_t SEND_INTERVAL_MS = 450;
const int STATUS_LED_PIN        = 2;

char targetServerIp[32] = "192.168.1.100";

const bool USE_ESP_NOW  = true;
const bool USE_REAL_CSI = true;
const uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

float latestRealCSI[32];
float prevCSI[32];
bool hasNewCSI = false;

float bandMovement[4] = {0};
float movementIntensity = 0.0;
float nodeConfidence = 0.6;
float activityLevel = 0;
bool significantObstruction = false;
int hotZoneCount = 0;
float csiVariance = 0;

WiFiUDP udp;
Preferences prefs;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
bool espnowReady = false;
int packetCount = 0;

void statusLed(bool on) {
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

#if USE_NEOPIXEL
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}
#else
void setRGB(uint8_t, uint8_t, uint8_t) {}
#endif

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void initESPNow() {
  // Must be after WiFi.mode(WIFI_STA)
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] init failed (non-fatal)");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_ADDRESS, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    espnowReady = true;
    Serial.println("[ESP-NOW] ready");
  } else {
    Serial.println("[ESP-NOW] peer add failed (non-fatal)");
  }
}

void updateRichCSIFeatures() {
  for (int b = 0; b < 4; b++) {
    int start = b * 8;
    float maxChange = 0;
    for (int i = 0; i < 8; i++) {
      int idx = start + i;
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
    float diff = latestRealCSI[i] - meanAll;
    varianceAll += diff * diff;
  }
  csiVariance = varianceAll / 32.0f;

  float strongestBand = 0;
  for (int b = 0; b < 4; b++) {
    if (bandMovement[b] > strongestBand) strongestBand = bandMovement[b];
  }

  movementIntensity = constrain(strongestBand * 4.8f + csiVariance * 1.6f, 0.0f, 1.0f);
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
    int8_t real = info->buf[i * 2];
    int8_t imag = info->buf[i * 2 + 1];
    float amp = sqrtf((float)real * real + (float)imag * imag);
    latestRealCSI[i] = amp / 200.0f;
    if (latestRealCSI[i] > 1.0f) latestRealCSI[i] = 1.0f;
  }
  for (int i = numSub; i < 32; i++) latestRealCSI[i] = 0.0f;

  hasNewCSI = true;
  updateRichCSIFeatures();
}

bool initRealCSI() {
  esp_err_t err;

  err = esp_wifi_set_promiscuous(true);
  if (err != ESP_OK) {
    Serial.printf("[CSI] promiscuous failed: %d\n", (int)err);
    return false;
  }

  wifi_csi_config_t csi_config = {};
  csi_config.lltf_en = true;
  csi_config.htltf_en = true;
  csi_config.stbc_htltf2_en = true;
  csi_config.ltf_merge_en = true;
  csi_config.manu_scale = false;

  err = esp_wifi_set_csi_config(&csi_config);
  if (err != ESP_OK) {
    Serial.printf("[CSI] config failed: %d (board may not support CSI)\n", (int)err);
    return false;
  }

  err = esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
  if (err != ESP_OK) {
    Serial.printf("[CSI] cb failed: %d\n", (int)err);
    return false;
  }

  err = esp_wifi_set_csi(true);
  if (err != ESP_OK) {
    Serial.printf("[CSI] enable failed: %d\n", (int)err);
    return false;
  }

  for (int i = 0; i < 32; i++) {
    latestRealCSI[i] = 0.3f;
    prevCSI[i] = 0.3f;
  }
  Serial.println("[CSI] enabled → UDP :4210");
  return true;
}

void connectWiFi() {
  prefs.begin("csi", true);
  String saved = prefs.getString("server_ip", String(targetServerIp));
  prefs.end();
  if (saved.length() > 0 && saved.length() < (int)sizeof(targetServerIp)) {
    saved.toCharArray(targetServerIp, sizeof(targetServerIp));
  }

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);

  WiFiManagerParameter custom_server(
      "server_ip",
      "Echo Grid / CSI host IP",
      targetServerIp,
      31
  );
  wifiManager.addParameter(&custom_server);

  String apName = String("ESP32-CSI-") + NODE_ID;

  if (wifiManager.autoConnect(apName.c_str())) {
    wifiConnected = true;

    const char* entered = custom_server.getValue();
    if (entered && strlen(entered) >= 7) {
      strncpy(targetServerIp, entered, sizeof(targetServerIp) - 1);
      targetServerIp[sizeof(targetServerIp) - 1] = '\0';
      prefs.begin("csi", false);
      prefs.putString("server_ip", targetServerIp);
      prefs.end();
    }

    Serial.print("[WiFi] OK → ");
    Serial.print(targetServerIp);
    Serial.println(":4210");
    statusLed(true);
  } else {
    Serial.println("[WiFi] portal timeout");
    wifiConnected = false;
  }
}

void sendCSIPacket() {
  float rssi = wifiConnected ? WiFi.RSSI() : -90;

  if (!hasNewCSI) {
    // soft simulated fallback so host still sees traffic
    for (int i = 0; i < 32; i++)
      latestRealCSI[i] = 0.35f + (random(30) / 100.0f);
    updateRichCSIFeatures();
  }

  if (espnowReady) {
    StaticJsonDocument<1600> doc;
    doc["node"] = NODE_ID;
    doc["timestamp"] = millis();
    doc["rssi"] = (int)rssi;
    doc["type"] = "wifi_csi";
    doc["activity"] = activityLevel;
    doc["movement_intensity"] = movementIntensity;
    doc["confidence"] = nodeConfidence;
    JsonArray csiArr = doc.createNestedArray("csi");
    for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);
    char jsonBuffer[1600];
    serializeJson(doc, jsonBuffer);
    esp_now_send(BROADCAST_ADDRESS, (uint8_t*)jsonBuffer, strlen(jsonBuffer));
  }

  if (wifiConnected) {
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

    char jsonBuffer[1600];
    size_t n = serializeJson(doc, jsonBuffer);

    if (udp.beginPacket(targetServerIp, TARGET_PORT)) {
      udp.write((uint8_t*)jsonBuffer, n);
      udp.endPacket();
    }
  }

  packetCount++;
  hasNewCSI = false;

  Serial.printf("[UDP] %s:%u act=%.2f mov=%.2f rssi=%.0f\n",
                targetServerIp, TARGET_PORT, activityLevel, movementIntensity, rssi);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== ESP32 CSI Node (stable) ===");

  pinMode(STATUS_LED_PIN, OUTPUT);
  statusLed(false);

#if USE_NEOPIXEL
  rgbLed.begin();
  rgbLed.setBrightness(60);
  setRGB(0, 0, 80);
#endif

  // Critical boot order for ESP32:
  // 1) WiFi STA mode
  // 2) ESP-NOW (optional)
  // 3) WiFiManager connect
  // 4) CSI (optional, soft-fail)
  WiFi.mode(WIFI_STA);
  delay(50);

  if (USE_ESP_NOW) initESPNow();

  connectWiFi();

  if (USE_REAL_CSI) {
    if (!initRealCSI()) {
      Serial.println("[CSI] hardware CSI unavailable — using soft CSI");
    }
  }

  udp.begin(4211);

  Serial.printf("Target %s:%u\n", targetServerIp, TARGET_PORT);
  Serial.println("Setup complete");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }
  delay(10);
}
