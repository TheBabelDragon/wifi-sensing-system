#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// === TFT Display for Cheap Yellow Display (ESP32-2432S028) ===
// Most popular and reliable library for these boards
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Pin definitions for ESP32-2432S028 (Cheap Yellow Display)
#define TOUCH_CS   33
#define TOUCH_IRQ  36

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// === USER CONFIGURATION ===
const char* TARGET_SERVER_IP  = "192.168.1.100";
const uint16_t TARGET_PORT    = 4210;
const char* NODE_ID           = "esp32_cyd_01";
const uint32_t SEND_INTERVAL_MS = 500;
const int STATUS_LED_PIN      = 2;

const bool USE_REAL_CSI       = true;

// CSI buffers
float latestRealCSI[32];
bool hasNewCSI = false;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;

// === Real CSI Callback ===
void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;
  int len = info->len;
  int numSub = min(32, len / 2);
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
    .lltf_en = true, .htltf_en = true, .stbc_htltf2_en = true,
    .ltf_merge_en = true, .channel_width = WIFI_BW_HT20, .manu_scale = false
  };
  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
  esp_wifi_set_csi(true);
  Serial.println("[CSI] Real CSI collection enabled");
}

// === Display Helpers ===
void drawHeader() {
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

void updateDisplay(float rssi, bool realCSI, int csiCount) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setCursor(10, 60);
  tft.print("WiFi: ");
  if (wifiConnected) {
    tft.setTextColor(TFT_GREEN);
    tft.print("Connected");
  } else {
    tft.setTextColor(TFT_RED);
    tft.print("Disconnected");
  }

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 90);
  tft.print("RSSI: ");
  tft.print(rssi);
  tft.print(" dBm");

  tft.setCursor(10, 120);
  tft.print("CSI: ");
  if (realCSI && hasNewCSI) {
    tft.setTextColor(TFT_CYAN);
    tft.print("REAL");
  } else {
    tft.setTextColor(TFT_YELLOW);
    tft.print("SIM");
  }

  tft.setTextSize(1);
  tft.setCursor(10, 150);
  tft.print("Packets sent: ");
  tft.println(csiCount);

  // Simple CSI bar visualization
  tft.fillRect(10, 180, 300, 40, TFT_BLACK);
  for (int i = 0; i < 32; i++) {
    int barHeight = latestRealCSI[i] * 35;
    tft.fillRect(10 + i*9, 220 - barHeight, 7, barHeight, TFT_CYAN);
  }
}

void connectWiFi() {
  WiFiManager wifiManager;
  String apName = "ESP32-CSI-CYD-" + String(NODE_ID);

  wifiManager.setAPCallback([](WiFiManager* myWiFiManager){
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(10, 60);
    tft.println("CONFIG MODE");
    tft.setCursor(10, 85);
    tft.println("Connect to:");
    tft.println(myWiFiManager->getConfigPortalSSID());
  });

  if (!wifiManager.autoConnect(apName.c_str())) {
    ESP.restart();
  }

  wifiConnected = true;
  Serial.println("WiFi connected via WiFiManager");

  if (USE_REAL_CSI) {
    initRealCSI();
  }
}

void sendCSIPacket(int &packetCount) {
  if (!wifiConnected) return;

  float rssi = WiFi.RSSI();

  if (USE_REAL_CSI && hasNewCSI) {
    // Use real CSI
  } else {
    // Fallback simulation
    for (int i = 0; i < 32; i++) {
      latestRealCSI[i] = 0.4 + random(40)/100.0;
    }
  }

  StaticJsonDocument<1024> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)rssi;
  doc["type"] = "wifi_csi";

  JsonArray csi = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csi.add(latestRealCSI[i]);

  char buffer[1024];
  serializeJson(doc, buffer);

  udp.beginPacket(TARGET_SERVER_IP, TARGET_PORT);
  udp.write((uint8_t*)buffer, strlen(buffer));
  udp.endPacket();

  packetCount++;
  hasNewCSI = false;

  // Update display
  updateDisplay(rssi, USE_REAL_CSI, packetCount);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize display
  tft.init();
  tft.setRotation(1);           // Landscape for 2.8"
  tft.fillScreen(TFT_BLACK);
  drawHeader();

  // Initialize touch (optional)
  ts.begin();

  pinMode(STATUS_LED_PIN, OUTPUT);

  connectWiFi();

  udp.begin(4211);

  tft.setCursor(10, 200);
  tft.setTextColor(TFT_GREEN);
  tft.println("CSI Node Ready");

  Serial.println("=== ESP32 CSI + Cheap Yellow Display Ready ===");
}

void loop() {
  static int packetCount = 0;
  unsigned long now = millis();

  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    sendCSIPacket(packetCount);
    lastSendTime = now;
  }

  // Simple touch handling (future expansion)
  if (ts.tirqTouched() && ts.touched()) {
    TS_Point p = ts.getPoint();
    // You can add touch actions here (e.g. force config portal)
  }

  delay(10);
}
