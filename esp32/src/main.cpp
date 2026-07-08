#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <cmath>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// ============================================================
// ESP32 CSI Node (Improved LED for WROOM-32UE boards)
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
const char* TARGET_SERVER_IP  = "192.168.1.100";
const uint16_t TARGET_PORT    = 4210;
const char* NODE_ID           = "esp32_node_01";
const uint32_t SEND_INTERVAL_MS = 500;
const int STATUS_LED_PIN      = 2;   // Onboard LED on most WROOM-32UE boards

const bool USE_REAL_CSI = true;

float latestRealCSI[32];
bool hasNewCSI = false;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int packetCount = 0;

float csiVariance = 0;
float activityLevel = 0;
bool significantObstruction = false;
int hotZoneCount = 0;

// === LED Control ===
unsigned long lastLedUpdate = 0;
int ledState = LOW;
int ledMode = 0; // 0 = slow blink (normal), 1 = fast blink (high activity)

void updateLED() {
  unsigned long now = millis();
  int interval = (ledMode == 1) ? 120 : 550;

  if (now - lastLedUpdate > interval) {
    lastLedUpdate = now;
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState);
  }
}

void setLedMode(int mode) {
  ledMode = mode;
}

// === CSI Processing ===
void updateCSIMetrics() {
  float mean = 0;
  for (int i = 0; i < 32; i++) mean += latestRealCSI[i];
  mean /= 32.0f;

  float variance = 0;
  for (int i = 0; i < 32; i++) {
    float diff = latestRealCSI[i] - mean;
    variance += diff * diff;
  }
  csiVariance = variance / 32.0f;

  activityLevel = constrain(csiVariance * 7.0f, 0.0f, 1.0f);
  hotZoneCount = constrain((int)(activityLevel * 5), 0, 5);
  significantObstruction = (hotZoneCount >= 3) || (activityLevel > 0.55f);

  // LED feedback
  setLedMode(significantObstruction ? 1 : 0);
}

// === Real CSI Callback ===
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
  updateCSIMetrics();
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

  Serial.println("[CSI] Real CSI collection enabled");
}

// === WiFiManager ===
void connectWiFi() {
  digitalWrite(STATUS_LED_PIN, HIGH); // Solid during boot/connect

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(45);

  String apName = String("ESP32-CSI-") + NODE_ID;

  if (wifiManager.autoConnect(apName.c_str())) {
    wifiConnected = true;
    Serial.println("WiFi connected");
    digitalWrite(STATUS_LED_PIN, LOW);
  } else {
    Serial.println("Running without main WiFi");
    digitalWrite(STATUS_LED_PIN, LOW);
  }
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float rssi = WiFi.RSSI();

  if (USE_REAL_CSI && hasNewCSI) {
    // Real data already processed
  } else {
    for (int i = 0; i < 32; i++) latestRealCSI[i] = 0.4f + (random(50) / 100.0f);
    updateCSIMetrics();
  }

  StaticJsonDocument<1024> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)rssi;
  doc["type"] = "wifi_csi";
  doc["activity"] = activityLevel;
  doc["hot_zones"] = hotZoneCount;
  doc["obstruction"] = significantObstruction;

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);

  char jsonBuffer[1024];
  serializeJson(doc, jsonBuffer);

  udp.beginPacket(TARGET_SERVER_IP, TARGET_PORT);
  udp.write((uint8_t*)jsonBuffer, strlen(jsonBuffer));
  udp.endPacket();

  packetCount++;
  hasNewCSI = false;

  // Quick LED flash when sending
  digitalWrite(STATUS_LED_PIN, HIGH);
  delay(25);
  digitalWrite(STATUS_LED_PIN, LOW);

  Serial.printf("[Sent] RSSI=%d | Act=%.2f | HZ=%d\n", (int)rssi, activityLevel, hotZoneCount);
}

void setup() {
  Serial.begin(115200);
  delay(150);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  connectWiFi();

  if (USE_REAL_CSI) {
    initRealCSI();
  }

  udp.begin(4211);

  Serial.println("=== ESP32 CSI Node Ready ===");
}

void loop() {
  unsigned long now = millis();

  updateLED();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }

  delay(10);
}
