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

// === Enhanced CSI Metrics for Obstruction Bodies ===
float csiVariance = 0;
float activityLevel = 0;
bool bodyDetected = false;

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

  activityLevel = constrain(csiVariance * 8.0f, 0.0f, 1.0f);
  bodyDetected = (activityLevel > 0.28f);
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

// === First-Person Perspective Visualization for CYD ===
#if HAS_DISPLAY

void drawFirstPersonView() {
  tft.fillScreen(TFT_BLACK);

  // Horizon line
  tft.drawFastHLine(0, 120, 320, TFT_DARKGREY);

  // Simple perspective floor grid
  for (int y = 120; y < 240; y += 20) {
    int width = map(y, 120, 240, 60, 300);
    tft.drawFastHLine(160 - width/2, y, width, TFT_DARKGREY);
  }

  // Draw "bodies" / obstructions in first-person view
  if (bodyDetected && activityLevel > 0.2f) {
    // Calculate apparent size and horizontal position based on activity
    int bodyWidth = map(activityLevel * 100, 0, 100, 20, 80);
    int bodyHeight = map(activityLevel * 100, 0, 100, 30, 120);
    int xPos = 160 + (int)((activityLevel - 0.5f) * 80);  // Slight horizontal shift

    // Draw body as a simple rectangle (like a person/obstacle)
    tft.fillRect(xPos - bodyWidth/2, 120 - bodyHeight, bodyWidth, bodyHeight, TFT_CYAN);
    tft.drawRect(xPos - bodyWidth/2, 120 - bodyHeight, bodyWidth, bodyHeight, TFT_WHITE);

    // Label
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.setCursor(xPos - 20, 125);
    tft.print("BODY");
  }

  // Status bar at top
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(5, 5);
  tft.printf("Node: %s  RSSI:%d  Act:%.2f", NODE_ID, (int)WiFi.RSSI(), activityLevel);

  if (bodyDetected) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(5, 18);
    tft.print("OBSTRUCTION DETECTED");
  }
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
    // Real CSI already processed
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

  Serial.printf("[UDP] Sent | RSSI=%d | Activity=%.2f | Body=%d\n", (int)rssi, activityLevel, bodyDetected);
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

  Serial.println("=== ESP32 CSI Node Ready (First-Person View) ===");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }

  delay(10);
}
