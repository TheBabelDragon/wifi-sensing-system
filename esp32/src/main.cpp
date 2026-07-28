#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <cmath>

// ============================================================
// ESP32 CSI closed-loop (standard + CYD)
//   CSI out  → broadcast :4210
//   commands ← UDP :4211  (echo_cmd from Echo Grid)
// ============================================================

#ifndef HAS_DISPLAY
#define HAS_DISPLAY 0
#endif

#ifndef NODE_ID_STR
#define NODE_ID_STR "esp32_node_01"
#endif

#if HAS_DISPLAY
  #include <TFT_eSPI.h>
  TFT_eSPI tft = TFT_eSPI();
#endif

const uint16_t CSI_PORT         = 4210;
const uint16_t CMD_PORT         = 4211;
const char* NODE_ID             = NODE_ID_STR;
const int STATUS_LED_PIN        = 2;
const bool USE_REAL_CSI         = true;

uint32_t sendIntervalMs         = 450;
uint32_t minIntervalMs          = 120;
uint32_t maxIntervalMs          = 1200;
float boostLevel                = 0.0f;

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

WiFiUDP udpCsi;
WiFiUDP udpCmd;
unsigned long lastSendTime = 0;
unsigned long lastUiTime = 0;
bool wifiConnected = false;
int packetCount = 0;
int cmdCount = 0;

void statusLed(bool on) {
#if !HAS_DISPLAY
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

#if HAS_DISPLAY
void initDisplay() {
  tft.init();
  tft.setRotation(1); // landscape on CYD
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(8, 8);
  tft.println("Echo CSI Node");
  tft.setTextSize(1);
  tft.setCursor(8, 32);
  tft.print("id: ");
  tft.println(NODE_ID);
  tft.setCursor(8, 48);
  tft.println("CSI:4210  CMD:4211");
}

void updateDisplay() {
  tft.fillRect(0, 70, 320, 170, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(8, 72);
  tft.print("WiFi: ");
  tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.print(wifiConnected ? "OK  " : "--  ");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("RSSI ");
  tft.print(wifiConnected ? (int)WiFi.RSSI() : 0);
  tft.println(" dBm");

  tft.setCursor(8, 90);
  tft.printf("rate %u ms   boost %.2f\n", sendIntervalMs, boostLevel);
  tft.setCursor(8, 106);
  tft.printf("act %.2f  mov %.2f  cmds %d\n", activityLevel, movementIntensity, cmdCount);
  tft.setCursor(8, 122);
  tft.printf("pkts %d\n", packetCount);

  // CSI bars
  for (int i = 0; i < 32; i++) {
    int h = (int)(latestRealCSI[i] * 50);
    if (h < 1) h = 1;
    if (h > 55) h = 55;
    uint16_t color = boostLevel > 0.3f ? TFT_ORANGE : TFT_CYAN;
    tft.fillRect(8 + i * 9, 200 - h, 7, h, color);
  }
}
#endif

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

  float gain = 1.0f + 0.8f * boostLevel;
  movementIntensity = constrain((strongest * 4.8f + csiVariance * 1.6f) * gain, 0.0f, 1.0f);
  activityLevel = constrain((csiVariance * 6.2f + movementIntensity * 0.9f) * gain, 0.0f, 1.0f);
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

void handleEchoCommand(const char* json, size_t len) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json, len)) return;
  const char* type = doc["type"] | "";
  if (strcmp(type, "echo_cmd") != 0) return;
  const char* cmd = doc["cmd"] | "";
  cmdCount++;

  if (strcmp(cmd, "set_rate") == 0) {
    uint32_t ms = doc["interval_ms"] | sendIntervalMs;
    sendIntervalMs = constrain(ms, minIntervalMs, maxIntervalMs);
    Serial.printf("[CMD] set_rate %u\n", sendIntervalMs);
  } else if (strcmp(cmd, "boost") == 0) {
    boostLevel = constrain((float)(doc["level"] | 0.7), 0.0f, 1.0f);
    sendIntervalMs = constrain((uint32_t)(450.0f - 280.0f * boostLevel), minIntervalMs, maxIntervalMs);
    Serial.printf("[CMD] boost %.2f rate=%u\n", boostLevel, sendIntervalMs);
  } else if (strcmp(cmd, "quiet") == 0) {
    boostLevel = 0.0f;
    sendIntervalMs = 900;
    Serial.println("[CMD] quiet");
  } else if (strcmp(cmd, "ping") == 0) {
    Serial.println("[CMD] ping");
  }
}

void pollCommands() {
  int packetSize = udpCmd.parsePacket();
  while (packetSize > 0) {
    char buf[512];
    int n = udpCmd.read(buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = 0;
      handleEchoCommand(buf, n);
    }
    packetSize = udpCmd.parsePacket();
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
  doc["interval_ms"] = sendIntervalMs;
  doc["boost"] = boostLevel;
  doc["cmd_count"] = cmdCount;

  JsonArray bandMov = doc.createNestedArray("band_movement");
  for (int b = 0; b < 4; b++) bandMov.add(bandMovement[b]);
  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);

  char buf[1600];
  size_t n = serializeJson(doc, buf);

  IPAddress bcast = WiFi.localIP();
  bcast[3] = 255;
  if (udpCsi.beginPacket(bcast, CSI_PORT)) {
    udpCsi.write((uint8_t*)buf, n);
    udpCsi.endPacket();
  }
  if (udpCsi.beginPacket(IPAddress(255, 255, 255, 255), CSI_PORT)) {
    udpCsi.write((uint8_t*)buf, n);
    udpCsi.endPacket();
  }

  packetCount++;
  hasNewCSI = false;
  Serial.printf("[UDP] %s :%u rate=%u boost=%.2f cmds=%d\n",
                NODE_ID, CSI_PORT, sendIntervalMs, boostLevel, cmdCount);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("=== ESP32 CSI closed-loop ===");
  Serial.printf("NODE_ID=%s  CSI:%u  CMD:%u  DISPLAY=%d\n",
                NODE_ID, CSI_PORT, CMD_PORT, HAS_DISPLAY);

#if HAS_DISPLAY
  initDisplay();
#else
  pinMode(STATUS_LED_PIN, OUTPUT);
  statusLed(false);
#endif

  WiFi.mode(WIFI_STA);
  delay(50);
  connectWiFi();

  if (USE_REAL_CSI && !initRealCSI()) {
    Serial.println("[CSI] soft fallback");
  }

  udpCsi.begin(4212);
  udpCmd.begin(CMD_PORT);
  Serial.printf("Listening Echo cmds :%u\n", CMD_PORT);

#if HAS_DISPLAY
  updateDisplay();
#endif
}

void loop() {
  pollCommands();

  if (millis() - lastSendTime >= sendIntervalMs) {
    sendCSIPacket();
    lastSendTime = millis();
  }

#if HAS_DISPLAY
  if (millis() - lastUiTime > 400) {
    updateDisplay();
    lastUiTime = millis();
  }
#endif
  delay(5);
}
