#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// ============================================================
// BOARD TYPE - Controlled by build flag HAS_DISPLAY
// Set via platformio.ini build_flags
// ============================================================

#if HAS_DISPLAY
  #include <TFT_eSPI.h>
  #include <XPT2046_Touchscreen.h>

  #define TOUCH_CS   33
  #define TOUCH_IRQ  36

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

// CSI buffers
float latestRealCSI[32];
bool hasNewCSI = false;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int packetCount = 0;

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
}

void initRealCSI() {
  esp_wifi_set_promiscuous(true);

  wifi_csi_config_t csi_config = {
    .lltf_en = true,
    .htltf_en = true,
    .stbc_htltf2_en = true,
    .ltf_merge_en = true,
    .channel_width = WIFI_BW_HT20,
    .manu_scale = false
  };

  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
  esp_wifi_set_csi(true);

  Serial.println("[CSI] Real CSI collection enabled");
}

// === Display (only for CYD) ===
#if HAS_DISPLAY
void initDisplay() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("ESP32 CSI Node");
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.print("Node: ");
  tft.println(NODE_ID);
}

void updateDisplay(float rssi) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 60);
  tft.print("WiFi: ");
  tft.setTextColor(wifiConnected ? TFT_GREEN : TFT_RED);
  tft.print(wifiConnected ? "Connected" : "Disconnected");

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.print("RSSI: ");
  tft.print(rssi);
  tft.print(" dBm");

  tft.setCursor(10, 120);
  tft.print("CSI Mode: ");
  tft.setTextColor(hasNewCSI && USE_REAL_CSI ? TFT_CYAN : TFT_YELLOW);
  tft.print(USE_REAL_CSI ? "REAL" : "SIM");

  // Simple bar graph
  tft.fillRect(10, 160, 300, 50, TFT_BLACK);
  for (int i = 0; i < 32; i++) {
    int h = latestRealCSI[i] * 45;
    tft.fillRect(12 + (i * 9), 205 - h, 6, h, TFT_CYAN);
  }
}
#endif

// === WiFiManager ===
void connectWiFi() {
  WiFiManager wifiManager;
  String apName = String("ESP32-CSI-") + NODE_ID;

  wifiManager.setAPCallback([](WiFiManager* myWiFiManager) {
    Serial.println("Config portal active");
  });

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
    // Real CSI data is already in latestRealCSI[]
  } else {
    for (int i = 0; i < 32; i++) {
      latestRealCSI[i] = 0.4f + (random(50) / 100.0f);
    }
  }

  StaticJsonDocument<1024> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)rssi;
  doc["type"] = "wifi_csi";

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) {
    csiArr.add(latestRealCSI[i]);
  }

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

  Serial.printf("[UDP] Sent | RSSI=%.0f | count=%d\n", rssi, packetCount);
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

  Serial.println("=== ESP32 CSI Node Ready ===");
  Serial.printf("Display: %s\n", HAS_DISPLAY ? "ENABLED (CYD)" : "DISABLED (Standard ESP32)");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket();
    lastSendTime = now;
  }

  delay(10);
}
