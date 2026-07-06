#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>   // tzapu/WiFiManager - automatic WiFi configuration via captive portal

// === USER CONFIGURATION ===
// NOTE: WiFi credentials are NO LONGER hardcoded.
// On first boot (or after resetSettings), the device creates a WiFi AP.
// Connect to it from your phone and configure real WiFi credentials via the web portal.

const char* TARGET_SERVER_IP  = "192.168.1.100";           // IP of machine running the Python ingestor
const uint16_t TARGET_PORT    = 4210;                      // UDP port (must match ingestion/ingestor.py)
const char* NODE_ID           = "esp32_mining_hall_01";    // Unique per physical node
const uint32_t SEND_INTERVAL_MS = 400;                     // How often to sample & send CSI
const int STATUS_LED_PIN      = 2;                         // Onboard LED

// === ADVANCED ===
const bool USE_REAL_CSI       = false;                     // Enable after adding real CSI callback
const uint16_t LOCAL_UDP_PORT = 4211;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int ledState = LOW;

float csiBuffer[32];
float lastRssi = -70;

void setupCSI() {
  for (int i = 0; i < 32; i++) {
    csiBuffer[i] = 0.35 + 0.4 * (random(100) / 100.0);
  }
}

void updateSimulatedCSI(float currentRssi) {
  float rssiNorm = (currentRssi + 90.0) / 60.0;
  if (rssiNorm < 0) rssiNorm = 0;
  if (rssiNorm > 1) rssiNorm = 1;

  for (int i = 0; i < 32; i++) {
    float drift = (random(200) - 100) / 2000.0;
    csiBuffer[i] = csiBuffer[i] * 0.85 + (0.15 * (0.25 + 0.55 * rssiNorm + drift));
    if (csiBuffer[i] < 0.08) csiBuffer[i] = 0.08;
    if (csiBuffer[i] > 0.92) csiBuffer[i] = 0.92;
  }
}

// === WiFiManager-based connection (replaces hardcoded WiFi.begin) ===
void connectWiFi() {
  WiFiManager wifiManager;

  // Uncomment the next line to force config portal on every boot (useful for testing)
  // wifiManager.resetSettings();

  // Custom AP name so you can identify the node easily when multiple devices are deployed
  String apName = "ESP32-CSI-" + String(NODE_ID);

  wifiManager.setAPCallback([](WiFiManager* myWiFiManager) {
    Serial.println("\n=== WiFiManager Config Portal ==");
    Serial.print("Connect to AP: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    Serial.print("Then open http://");
    Serial.println(WiFi.softAPIP());
    Serial.println("Enter your real WiFi credentials.");
  });

  // Set timeout so it doesn't hang forever if user doesn't configure
  wifiManager.setConfigPortalTimeout(180); // 3 minutes

  Serial.println("Starting WiFiManager...");

  if (!wifiManager.autoConnect(apName.c_str())) {
    Serial.println("Failed to connect to WiFi after timeout. Restarting...");
    delay(3000);
    ESP.restart();
  }

  wifiConnected = true;
  Serial.println("\n=== WiFi Connected via WiFiManager ===");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float currentRssi = WiFi.RSSI();
  lastRssi = currentRssi;

  if (USE_REAL_CSI) {
    // TODO: populate from real CSI callback
  }
  updateSimulatedCSI(currentRssi);

  StaticJsonDocument<1536> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)currentRssi;
  doc["type"] = "wifi_csi";

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) {
    csiArr.add(csiBuffer[i]);
  }

  JsonArray linksArr = doc.createNestedArray("links");
  JsonObject linkObj = linksArr.createNestedObject();
  linkObj["node_a"] = NODE_ID;
  linkObj["node_b"] = "local_ap";
  linkObj["weight"] = (currentRssi + 100.0) / 100.0;

  char jsonStr[1536];
  size_t len = serializeJson(doc, jsonStr);

  udp.beginPacket(TARGET_SERVER_IP, TARGET_PORT);
  udp.write((uint8_t*)jsonStr, len);
  bool sent = udp.endPacket();

  if (sent) {
    Serial.print("[UDP] Sent CSI packet | RSSI=");
    Serial.print((int)currentRssi);
    Serial.print(" | node=");
    Serial.println(NODE_ID);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(30);
    digitalWrite(STATUS_LED_PIN, HIGH);
  } else {
    Serial.println("[UDP] Send failed");
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("WiFi connection lost. WiFiManager will handle reconnection...");
    // WiFiManager + ESP32 stack usually handles auto-reconnect well.
    // We just mark it so sendCSIPacket() pauses until reconnected.
    digitalWrite(STATUS_LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 CSI UDP Sender v3.0 (WiFiManager) ===");
  Serial.println("WiFi Spatial Intelligence System - Real Hardware Mode");

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  setupCSI();
  connectWiFi();

  if (wifiConnected) {
    udp.begin(LOCAL_UDP_PORT);
    Serial.print("UDP ready. Targeting ");
    Serial.print(TARGET_SERVER_IP);
    Serial.print(":");
    Serial.println(TARGET_PORT);
  }

  Serial.println("Setup complete. Starting CSI transmission...\n");
}

void loop() {
  unsigned long now = millis();

  checkWiFi();

  if (wifiConnected && (now - lastSendTime >= SEND_INTERVAL_MS)) {
    sendCSIPacket();
    lastSendTime = now;
  }

  // Status LED heartbeat when connected
  static unsigned long lastLed = 0;
  if (now - lastLed > 2000) {
    if (wifiConnected) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
    }
    lastLed = now;
  }

  delay(10);
}

/*
================================================================================
 FULL REAL CSI IMPLEMENTATION GUIDE
================================================================================

#include <esp_wifi.h>
#include <esp_wifi_types.h>

float latestRealCSI[32];
bool hasNewCSI = false;

void csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
  if (!info || !info->buf) return;

  int numSub = min(32, (int)(info->len / 2));
  for (int i = 0; i < numSub && i < 32; i++) {
    int8_t real = info->buf[i*2];
    int8_t imag = info->buf[i*2 + 1];
    float amp = sqrt( (float)real*real + (float)imag*imag );
    latestRealCSI[i] = amp / 200.0;
    if (latestRealCSI[i] > 1.0) latestRealCSI[i] = 1.0;
  }
  hasNewCSI = true;
}

// In setup() after WiFi is connected:
//   esp_wifi_set_promiscuous(true);
//   wifi_csi_config_t csi_cfg = { .lltf_en = 1, .htltf_en = 1, .channel_width = WIFI_BW_HT20 };
//   esp_wifi_set_csi_config(&csi_cfg);
//   esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
//   esp_wifi_set_csi(true);

// Then in sendCSIPacket():
//   if (USE_REAL_CSI && hasNewCSI) { memcpy(csiBuffer, latestRealCSI, sizeof(csiBuffer)); hasNewCSI = false; }
================================================================================
*/
