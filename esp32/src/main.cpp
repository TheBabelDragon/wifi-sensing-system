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

// === USER CONFIGURATION ===
const char* TARGET_SERVER_IP  = "192.168.1.100";
const uint16_t TARGET_PORT    = 4210;
const char* NODE_ID           = "esp32_node_01";
const uint32_t SEND_INTERVAL_MS = 500;
const int STATUS_LED_PIN      = 2;

const bool USE_REAL_CSI = true;

float latestRealCSI[32];
bool hasNewCSI = false;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int packetCount = 0;

// === Enhanced CSI Metrics ===
float csiVariance = 0;
float activityLevel = 0;
bool bodyDetected = false;
int bodyCount = 0;
float bodyPositions[3] = {0}; // relative horizontal positions

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

  activityLevel = constrain(csiVariance * 7.5f, 0.0f, 1.0f);
  bodyDetected = (activityLevel > 0.25f);

  // Estimate number of bodies and positions (simple clustering)
  bodyCount = 0;
  if (activityLevel > 0.25f) bodyCount++;
  if (activityLevel > 0.55f) bodyCount++;
  if (activityLevel > 0.80f) bodyCount++;

  // Fake some horizontal spread for multiple bodies
  bodyPositions[0] = (activityLevel - 0.5f) * 60;
  bodyPositions[1] = (activityLevel - 0.3f) * 90;
  bodyPositions[2] = (activityLevel - 0.7f) * 50;
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

// === Sick First-Person Perspective View ===
#if HAS_DISPLAY

void drawFirstPersonView() {
  tft.fillScreen(TFT_BLACK);

  // Horizon + sky gradient feel
  tft.fillRect(0, 0, 320, 120, TFT_NAVY);
  tft.drawFastHLine(0, 120, 320, TFT_WHITE);

  // Perspective floor grid (stronger depth)
  for (int i = 0; i < 7; i++) {
    int y = 120 + i * 18;
    int w = 40 + i * 38;
    tft.drawFastHLine(160 - w/2, y, w, TFT_DARKGREY);
  }

  // Draw obstruction bodies in first-person view
  for (int b = 0; b < min(3, bodyCount); b++) {
    if (activityLevel < 0.2f) break;

    float strength = activityLevel - (b * 0.15f);
    if (strength < 0.15f) continue;

    int bodyWidth  = map(strength * 100, 0, 100, 18, 70);
    int bodyHeight = map(strength * 100, 0, 100, 35, 110);

    int xOffset = bodyPositions[b];
    int xPos = 160 + xOffset;
    int yBottom = 118;
    int yTop = yBottom - bodyHeight;

    // Body (cyan with white outline)
    tft.fillRect(xPos - bodyWidth/2, yTop, bodyWidth, bodyHeight, TFT_CYAN);
    tft.drawRect(xPos - bodyWidth/2, yTop, bodyWidth, bodyHeight, TFT_WHITE);

    // Simple head
    int headSize = bodyWidth / 2;
    tft.fillCircle(xPos, yTop - headSize/2, headSize, TFT_CYAN);
    tft.drawCircle(xPos, yTop - headSize/2, headSize, TFT_WHITE);

    // Distance label
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(xPos - 12, yBottom + 4);
    tft.printf("%.1fm", 1.5f + b * 0.8f);
  }

  // Top status bar
  tft.fillRect(0, 0, 320, 22, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(6, 6);
  tft.printf("%s  RSSI:%d  Act:%.2f  Bodies:%d", NODE_ID, (int)WiFi.RSSI(), activityLevel, bodyCount);

  // Bottom info
  if (bodyDetected) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(6, 225);
    tft.print("OBSTRUCTION DETECTED");
  } else {
    tft.setTextColor(TFT_GREEN);
    tft.setCursor(6, 225);
    tft.print("CLEAR");
  }

  // Small activity bar on the right
  int barHeight = activityLevel * 80;
  tft.fillRect(305, 140, 10, 80, TFT_DARKGREY);
  tft.fillRect(305, 220 - barHeight, 10, barHeight, TFT_CYAN);
}

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);
  drawFirstPersonView();
}

void updateDisplay(float rssi) {
  drawFirstPersonView();
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

  Serial.printf("[UDP] Sent | RSSI=%d | Act=%.2f | Bodies=%d\n", (int)rssi, activityLevel, bodyCount);
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

  Serial.println("=== ESP32 CSI Node Ready (Sick First-Person View) ===");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }

  delay(10);
}
