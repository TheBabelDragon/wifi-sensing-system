#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <cmath>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// ============================================================
// Goal: Intuitive "heartbeat detector" style visualization
// Hotspots should primarily appear from real physical bodies/movement
// No repeating deterministic patterns
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
const uint32_t SEND_INTERVAL_MS = 380;
const int STATUS_LED_PIN      = 2;

const bool USE_REAL_CSI = true;

float latestRealCSI[32];
bool hasNewCSI = false;
float prevCSI[32];

float csiVariance = 0;
float activityLevel = 0;
bool significantObstruction = false;
int hotZoneCount = 0;

#define GRID_W 20
#define GRID_H 16
float heatMap[GRID_W * GRID_H];

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int packetCount = 0;

// === Improved Emergent Heat Map ===
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

  hotZoneCount = 0;

  for (int i = 0; i < GRID_W * GRID_H; i++) {
    int csiIdx = (i * 3) % 32;   // Less patterned distribution

    float current = latestRealCSI[csiIdx];
    float previous = prevCSI[csiIdx];

    // Focus more on change (movement) than raw amplitude
    float change = fabs(current - previous);
    float excitation = change * 3.5f;

    // Only boost if there's real global activity (reduces noise)
    if (activityLevel > 0.3f) {
      excitation += current * 0.6f;
    }

    // Very gentle neighbor diffusion
    if (i > 0)               excitation += heatMap[i-1] * 0.012f;
    if (i < GRID_W*GRID_H-1) excitation += heatMap[i+1] * 0.012f;

    // Decay + update
    heatMap[i] = heatMap[i] * 0.82f + excitation * 0.18f;
    heatMap[i] = constrain(heatMap[i], 0.0f, 2.0f);

    if (heatMap[i] > 1.05f) hotZoneCount++;
  }

  for (int i = 0; i < 32; i++) {
    prevCSI[i] = latestRealCSI[i];
  }

  significantObstruction = (hotZoneCount >= 4) || (activityLevel > 0.52f);
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

  for (int i = 0; i < 32; i++) prevCSI[i] = 0.3f;

  Serial.println("[CSI] Real CSI collection enabled");
}

// === Heat Map Display ===
#if HAS_DISPLAY

uint16_t heatColor(float v) {
  v = constrain(v, 0.0f, 1.9f);
  if (v < 0.3f)  return tft.color565(0, 0, (int)(v * 110));
  if (v < 0.6f)  return tft.color565(0, (int)(v * 190), 90);
  if (v < 1.0f)  return tft.color565((int)(v * 255), 170, 15);
  return tft.color565(255, (int)((v - 1.0f) * 140), 0);
}

void drawHeatMap() {
  tft.fillScreen(TFT_BLACK);

  const int cellW = 16;
  const int cellH = 15;

  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      int idx = y * GRID_W + x;
      float val = heatMap[idx];
      uint16_t col = heatColor(val);
      tft.fillRect(x * cellW, y * cellH, cellW-1, cellH-1, col);
    }
  }

  tft.fillRect(0, 0, 320, 20, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.printf("%s  RSSI:%d  Heat:%.2f  HotZones:%d", NODE_ID, (int)WiFi.RSSI(), activityLevel, hotZoneCount);

  if (significantObstruction) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(5, 225);
    tft.print("OBSTRUCTION DETECTED");
  } else {
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(5, 225);
    tft.print("CLEAR");
  }
}

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);

  for (int i = 0; i < GRID_W * GRID_H; i++) heatMap[i] = 0.02f;

  drawHeatMap();
}

void updateDisplay(float rssi) {
  drawHeatMap();
}

#endif

// === WiFiManager ===
void connectWiFi() {
  WiFiManager wifiManager;
  String apName = String("ESP32-CSI-") + NODE_ID;

  if (!wifiManager.autoConnect(apName.c_str())) {
    Serial.println("Failed to connect. Restarting...");
    delay(3000);
    ESP.restart();
  }

  wifiConnected = true;
  Serial.println("WiFi connected via WiFiManager");

  if (USE_REAL_CSI) {
    initRealCSI();
  }

  #if HAS_DISPLAY
    updateDisplay(WiFi.RSSI());
  #endif
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float rssi = WiFi.RSSI();

  if (USE_REAL_CSI && hasNewCSI) {
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
  doc["significant_obstruction"] = significantObstruction;
  doc["hot_zones"] = hotZoneCount;

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);

  char jsonBuffer[1024];
  serializeJson(doc, jsonBuffer);

  udp.beginPacket(TARGET_SERVER_IP, TARGET_PORT);
  udp.write((uint8_t*)jsonBuffer, strlen(jsonBuffer));
  udp.endPacket();

  packetCount++;
  hasNewCSI = false;

  #if HAS_DISPLAY
    updateDisplay(rssi);
  #endif

  Serial.printf("[UDP] Sent | RSSI=%d | Heat=%.2f | HotZones=%d\n", (int)rssi, activityLevel, hotZoneCount);
}

void setup() {
  Serial.begin(115200);
  delay(150);

  #if HAS_DISPLAY
    initDisplay();
  #endif

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  connectWiFi();

  udp.begin(4211);

  Serial.println("=== ESP32 CSI Heat Map (Heartbeat Style) ===");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }

  delay(10);
}
