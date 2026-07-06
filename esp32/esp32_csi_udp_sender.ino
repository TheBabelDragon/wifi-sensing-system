#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// === USER CONFIGURATION - EDIT BEFORE FLASHING ===
const char* WIFI_SSID         = "YOUR_WIFI_SSID";          // 2.4 GHz only
const char* WIFI_PASSWORD     = "YOUR_WIFI_PASSWORD";
const char* TARGET_SERVER_IP  = "192.168.1.100";           // IP of machine running docker/python ingestion/ingestor.py
const uint16_t TARGET_PORT    = 4210;                      // Must match ingestor UDP port
const char* NODE_ID           = "esp32_mining_hall_01";    // Unique per physical node; match config.yaml or tracking needs
const uint32_t SEND_INTERVAL_MS = 400;                     // Sample & transmit rate (ms). Lower = more responsive, higher bandwidth
const int STATUS_LED_PIN      = 2;                         // onboard LED (GPIO2 common on ESP32 dev boards)

// === ADVANCED / OPTIONAL ===
const bool USE_REAL_CSI       = false;                     // Set true after implementing full CSI callback (see comments at bottom)
const uint16_t LOCAL_UDP_PORT = 4211;                      // Local port if needed for future bidirectional

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int ledState = LOW;

// Simple moving average / variation for demo CSI (replace with real when USE_REAL_CSI=true)
float csiBuffer[32];
float lastRssi = -70;

void setupCSI() {
  // Initialize plausible starting CSI amplitudes (0.1 - 0.9 range like simulation)
  for (int i = 0; i < 32; i++) {
    csiBuffer[i] = 0.35 + 0.4 * (random(100) / 100.0);
  }
}

void updateSimulatedCSI(float currentRssi) {
  // Make CSI vary realistically with RSSI and small random walk (mimics environmental changes / motion)
  float rssiNorm = (currentRssi + 90.0) / 60.0; // rough -90 to -30 -> 0 to 1
  if (rssiNorm < 0) rssiNorm = 0;
  if (rssiNorm > 1) rssiNorm = 1;

  for (int i = 0; i < 32; i++) {
    // base + rssi influence + small noise + slow drift
    float drift = (random(200) - 100) / 2000.0;  // small random walk
    csiBuffer[i] = csiBuffer[i] * 0.85 + (0.15 * (0.25 + 0.55 * rssiNorm + drift));
    // clamp
    if (csiBuffer[i] < 0.08) csiBuffer[i] = 0.08;
    if (csiBuffer[i] > 0.92) csiBuffer[i] = 0.92;
  }
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
    // Blink LED while connecting
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    digitalWrite(STATUS_LED_PIN, HIGH); // solid on when connected
  } else {
    wifiConnected = false;
    Serial.println("\nFailed to connect to WiFi. Will retry...");
    digitalWrite(STATUS_LED_PIN, LOW);
  }
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float currentRssi = WiFi.RSSI();
  lastRssi = currentRssi;

  if (USE_REAL_CSI) {
    // TODO: copy from global populated by csi_rx_cb
    // For now falls through to simulated
  }
  updateSimulatedCSI(currentRssi);

  // Build JSON packet matching ingestion/ingestor.py + simulation expectations
  StaticJsonDocument<1536> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();                    // simple; replace with NTP unix ms for prod
  doc["rssi"] = (int)currentRssi;
  doc["type"] = "wifi_csi";

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) {
    csiArr.add(csiBuffer[i]);
  }

  JsonArray linksArr = doc.createNestedArray("links");
  JsonObject linkObj = linksArr.createNestedObject();
  linkObj["node_a"] = NODE_ID;
  linkObj["node_b"] = "local_ap";                // or sensed transmitter MAC / name
  linkObj["weight"] = (currentRssi + 100.0) / 100.0; // rough quality 0-1

  char jsonStr[1536];
  size_t len = serializeJson(doc, jsonStr);

  // Send UDP
  udp.beginPacket(TARGET_SERVER_IP, TARGET_PORT);
  udp.write((uint8_t*)jsonStr, len);
  bool sent = udp.endPacket();

  if (sent) {
    Serial.print("[UDP] Sent CSI packet (len=");
    Serial.print(len);
    Serial.print(") RSSI=");
    Serial.print((int)currentRssi);
    Serial.print(" node=");
    Serial.println(NODE_ID);
    // Quick LED blink on successful send
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
    Serial.println("WiFi lost. Reconnecting...");
    digitalWrite(STATUS_LED_PIN, LOW);
    connectWiFi();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 CSI UDP Sender v2.0 ===");
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

  // Heartbeat / status LED slow blink when idle but connected
  static unsigned long lastLed = 0;
  if (now - lastLed > 2000) {
    if (wifiConnected) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
    }
    lastLed = now;
  }

  // Small delay to keep loop responsive
  delay(10);
}

/*
================================================================================
 FULL REAL CSI IMPLEMENTATION GUIDE (uncomment & extend when ready)
================================================================================

#include <esp_wifi.h>
#include <esp_wifi_types.h>

// Global for latest CSI
float latestRealCSI[32];
bool hasNewCSI = false;

void csi_rx_cb(void *ctx, wifi_csi_info_t *info) {
  if (!info || !info->buf) return;

  // Example parsing (highly hardware/version dependent - test on your board!)
  // Common: info->len gives bytes; for HT20 often ~56-112 bytes for I/Q or legacy format.
  // Many projects take first N subcarriers or every 2nd value and compute amplitude.
  int numSub = min(32, (int)(info->len / 2)); // rough
  for (int i = 0; i < numSub && i < 32; i++) {
    int8_t real = info->buf[i*2];
    int8_t imag = info->buf[i*2 + 1];
    float amp = sqrt( (float)real*real + (float)imag*imag );
    latestRealCSI[i] = amp / 200.0; // normalize roughly to ~0-1 range (tune!)
    if (latestRealCSI[i] > 1.0) latestRealCSI[i] = 1.0;
  }
  hasNewCSI = true;

  // You can also use info->rx_ctrl.rssi for per-packet RSSI
}

// In setup(), AFTER WiFi connected and before udp:
//   esp_wifi_set_promiscuous(true);
//   wifi_csi_config_t csi_cfg = { .lltf_en = 1, .htltf_en = 1, .channel_width = WIFI_BW_HT20 };
//   esp_wifi_set_csi_config(&csi_cfg);
//   esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
//   esp_wifi_set_csi(true);
//   Serial.println("Real CSI callback registered");

// Then in updateSimulatedCSI or sendCSIPacket:
//   if (USE_REAL_CSI && hasNewCSI) {
//     memcpy(csiBuffer, latestRealCSI, sizeof(csiBuffer));
//     hasNewCSI = false;
//   }

// WARNING: CSI config is sensitive to WiFi mode (must be on same channel as AP),
//          may require station connected to specific AP, and behavior differs across
//          ESP32 / S2 / S3 / C3 silicon + arduino-esp32 core version.
//          Start with USE_REAL_CSI=false, validate UDP flow, then experiment.
//          For production, many teams use ESP-IDF examples or maintained CSI libs.
================================================================================
*/
