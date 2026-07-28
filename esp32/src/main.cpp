#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>
#include <cmath>
#include <Preferences.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// ============================================================
// ESP32 CSI Node — streams to Echo Grid / wifi-sensing host
// UDP port 4210 is the canonical contract.
// ============================================================

#if HAS_DISPLAY
  #include <TFT_eSPI.h>
  #include <XPT2046_Touchscreen.h>

  #define TOUCH_CS   33
  #define TOUCH_IRQ  36
  #define TFT_BL     21

  TFT_eSPI tft = TFT_eSPI();
  XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
#endif

// === CONFIG ===
// Port is fixed — matches Echo Grid CSIBridge + CSIIngestor
const uint16_t TARGET_PORT      = 4210;
const char* NODE_ID             = "esp32_node_01";
const uint32_t SEND_INTERVAL_MS = 450;
const int STATUS_LED_PIN        = 2;

// Server IP is loaded from NVS / WiFiManager (default below on first boot)
char targetServerIp[32] = "192.168.1.100";

const int RGB_LED_PIN = 2;
const int NUM_PIXELS  = 1;
Adafruit_NeoPixel rgbLed(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

const bool USE_ESP_NOW = true;
const bool USE_REAL_CSI = true;
const uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

float latestRealCSI[32];
float prevCSI[32];
bool hasNewCSI = false;

float bandMovement[4] = {0};
float bandVariance[4] = {0};
float movementIntensity = 0.0;
float nodeConfidence  = 0.6;

WiFiUDP udp;
Preferences prefs;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
bool espnowReady = false;
int packetCount = 0;

float csiVariance = 0;
float activityLevel = 0;
bool significantObstruction = false;
int hotZoneCount = 0;

void setRGB(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

void updateRGBStatus() {
  if (significantObstruction) {
    setRGB(255, 0, 0);
  } else if (movementIntensity > 0.6) {
    setRGB(255, 100, 0);
  } else if (activityLevel > 0.4) {
    setRGB(255, 200, 0);
  } else if (!wifiConnected && espnowReady) {
    setRGB(0, 150, 255);
  } else if (!wifiConnected) {
    setRGB(0, 0, 255);
  } else if (nodeConfidence > 0.75) {
    setRGB(0, 255, 200);
  } else {
    setRGB(0, 180, 0);
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void initESPNow() {
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(onDataSent);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_ADDRESS, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) == ESP_OK) espnowReady = true;
}

void sendViaESPNow() {
  if (!espnowReady) return;

  StaticJsonDocument<1600> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)WiFi.RSSI();
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
  serializeJson(doc, jsonBuffer);
  esp_now_send(BROADCAST_ADDRESS, (uint8_t*)jsonBuffer, strlen(jsonBuffer));
}

void updateRichCSIFeatures() {
  for (int b = 0; b < 4; b++) {
    int start = b * 8;
    float mean = 0;
    float maxChange = 0;
    float var = 0;

    for (int i = 0; i < 8; i++) {
      int idx = start + i;
      mean += latestRealCSI[idx];
      float change = fabs(latestRealCSI[idx] - prevCSI[idx]);
      if (change > maxChange) maxChange = change;
    }
    mean /= 8.0f;

    for (int i = 0; i < 8; i++) {
      int idx = start + i;
      float diff = latestRealCSI[idx] - mean;
      var += diff * diff;
    }

    bandMovement[b] = maxChange;
    bandVariance[b] = var / 8.0f;
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

  static float prevMovement = 0;
  float consistency = 1.0f - fabs(movementIntensity - prevMovement);
  nodeConfidence = constrain(0.4f + consistency * 0.5f + (packetCount / 80.0f) * 0.3f, 0.35f, 0.92f);
  prevMovement = movementIntensity;

  updateRGBStatus();
  for (int i = 0; i < 32; i++) prevCSI[i] = latestRealCSI[i];
}

void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;

  int numSub = min(32, info->len / 2);
  for (int i = 0; i < numSub; i++) {
    int8_t real = info->buf[i*2];
    int8_t imag = info->buf[i*2 + 1];
    float amp = sqrtf(real*real + imag*imag);
    latestRealCSI[i] = amp / 200.0f;
    if (latestRealCSI[i] > 1.0f) latestRealCSI[i] = 1.0f;
  }
  hasNewCSI = true;
  updateRichCSIFeatures();
}

void initRealCSI() {
  esp_wifi_set_promiscuous(true);

  wifi_csi_config_t csi_config = {
    .lltf_en = true,
    .htltf_en = true,
    .stbc_htltf2_en = true,
    .ltf_merge_en = true,
    .manu_scale = false
  };

  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
  esp_wifi_set_csi(true);

  for (int i = 0; i < 32; i++) prevCSI[i] = 0.3f;
  Serial.println("[CSI] Real CSI ready → UDP :4210");
}

void connectWiFi() {
  setRGB(0, 0, 255);

  prefs.begin("csi", true);
  String saved = prefs.getString("server_ip", targetServerIp);
  prefs.end();
  saved.toCharArray(targetServerIp, sizeof(targetServerIp));

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(120);

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
    if (entered && strlen(entered) > 0) {
      strncpy(targetServerIp, entered, sizeof(targetServerIp) - 1);
      targetServerIp[sizeof(targetServerIp) - 1] = '\0';
      prefs.begin("csi", false);
      prefs.putString("server_ip", targetServerIp);
      prefs.end();
    }

    Serial.print("[WiFi] connected. CSI → ");
    Serial.print(targetServerIp);
    Serial.println(":4210");
    setRGB(0, 180, 0);
  } else {
    Serial.println("[WiFi] portal timeout — ESP-NOW only");
    setRGB(0, 150, 255);
  }
}

void sendCSIPacket() {
  float rssi = WiFi.RSSI();

  if (!USE_REAL_CSI) {
    for (int i = 0; i < 32; i++) latestRealCSI[i] = 0.45f + (random(40) / 100.0f);
    updateRichCSIFeatures();
  }

  if (USE_ESP_NOW && espnowReady) {
    sendViaESPNow();
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
    serializeJson(doc, jsonBuffer);

    udp.beginPacket(targetServerIp, TARGET_PORT);
    udp.write((uint8_t*)jsonBuffer, strlen(jsonBuffer));
    udp.endPacket();
  }

  packetCount++;
  hasNewCSI = false;

  setRGB(255, 255, 255);
  delay(25);
  updateRGBStatus();

  Serial.printf("[UDP:%u] %s | Act=%.2f Mov=%.2f\n",
                TARGET_PORT, targetServerIp, activityLevel, movementIntensity);
}

void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  rgbLed.begin();
  rgbLed.setBrightness(80);
  setRGB(255, 0, 255);

  if (USE_ESP_NOW) initESPNow();
  connectWiFi();
  if (USE_REAL_CSI) initRealCSI();

  udp.begin(4211);

  Serial.println("=== ESP32 CSI Node ready ===");
  Serial.printf("Target %s:%u (Echo Grid / CSI host)\n", targetServerIp, TARGET_PORT);
}

void loop() {
  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }
  delay(10);
}
