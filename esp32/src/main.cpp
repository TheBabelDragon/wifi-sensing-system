#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <cmath>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// ============================================================
// BOARD TYPE - Controlled by build flag HAS_DISPLAY
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
const uint32_t SEND_INTERVAL_MS = 400;
const int STATUS_LED_PIN      = 2;

const bool USE_REAL_CSI = true;

// === CSI DATA ===
float latestRealCSI[32];
bool hasNewCSI = false;

// === ENHANCED METRICS ===
float csiVariance = 0;
float activityLevel = 0;
bool bodyDetected = false;
int bodyCount = 0;

// === HEAT MAP (Waveform Obstruction Spaces) ===
#define GRID_W 20
#define GRID_H 16
float heatMap[GRID_W * GRID_H];

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int packetCount = 0;

// === CSI Processing + Heat Map Update ===
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

  activityLevel = constrain(csiVariance * 7.2f, 0.0f, 1.0f);
  bodyDetected = (activityLevel > 0.26f);
  bodyCount = constrain((int)(activityLevel * 3.5f), 0, 3);

  // === Update Heat Map (Obstruction Waveform Spaces) ===
  // Map 32 CSI values across the 20x16 grid
  for (int i = 0; i < GRID_W * GRID_H; i++) {
    int csiIndex = i % 32;
    float base = latestRealCSI[csiIndex];

    // Excite cells based on CSI + global activity
    float excitation = base * 0.7f + activityLevel * 0.9f;

    // Add localized hot spots when bodies are detected
    if (bodyDetected) {
      int hotX = (i % GRID_W);
      int hotY = (i / GRID_W);
      float dist = abs(hotX - 10) + abs(hotY - 8); // center bias
      if (dist < 6) excitation += (0.6f - dist * 0.08f) * activityLevel;
    }

    // Simple persistence + decay
    heatMap[i] = heatMap[i] * 0.82f + excitation * 0.18f;
    heatMap[i] = constrain(heatMap[i], 0.0f, 1.8f);
  }
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

// === Heat Map Visualization (Obstruction Waveform Spaces) ===
#if HAS_DISPLAY

uint16_t heatColor(float v) {
  v = constrain(v, 0.0f, 1.6f);
  if (v < 0.3f) return tft.color565(0, 0, (int)(v * 180));           // Deep blue
  if (v < 0.6f) return tft.color565(0, (int)(v * 200), 180);           // Cyan
  if (v < 1.0f) return tft.color565((int)(v * 220), 220, 0);           // Yellow
  return tft.color565(255, (int)((v - 1.0f) * 200), 0);                // Hot white/red
}

void drawHeatMap() {
  tft.fillScreen(TFT_BLACK);

  const int cellW = 16;
  const int cellH = 15;
  const int startX = 0;
  const int startY = 0;

  for (int y = 0; y < GRID_H; y++) {
    for (int x = 0; x < GRID_W; x++) {
      int idx = y * GRID_W + x;
      float val = heatMap[idx];

      uint16_t col = heatColor(val);
      tft.fillRect(startX + x * cellW, startY + y * cellH, cellW - 1, cellH - 1, col);
    }
  }

  // Status overlay
  tft.fillRect(0, 0, 320, 20, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.printf("%s  RSSI:%d  Heat:%.2f  Bodies:%d", NODE_ID, (int)WiFi.RSSI(), activityLevel, bodyCount);

  if (bodyDetected) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(5, 225);
    tft.print("HOTSPOTS DETECTED - OBSTRUCTION");
  } else {
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(5, 225);
    tft.print("CLEAR - NO SIGNIFICANT OBSTRUCTION");
  }
}

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);

  // Initialize heat map
  for (int i = 0; i < GRID_W * GRID_H; i++) heatMap[i] = 0.1f;

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
  doc["body_detected"] = bodyDetected;
  doc["body_count"] = bodyCount;

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

  Serial.printf("[UDP] Sent | RSSI=%d | Heat=%.2f | Bodies=%d\n", (int)rssi, activityLevel, bodyCount);
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

  Serial.println("=== ESP32 CSI Heat Map Node Ready ===");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }

  delay(8);
}
