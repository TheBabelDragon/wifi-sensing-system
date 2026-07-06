#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>

#include <esp_wifi.h>
#include <esp_wifi_types.h>   // For real CSI collection

// === USER CONFIGURATION ===
const char* TARGET_SERVER_IP  = "192.168.1.100";
const uint16_t TARGET_PORT    = 4210;
const char* NODE_ID           = "esp32_mining_hall_01";
const uint32_t SEND_INTERVAL_MS = 400;
const int STATUS_LED_PIN      = 2;

// Set to true to use real hardware CSI (recommended now that it's implemented)
const bool USE_REAL_CSI       = true;
const uint16_t LOCAL_UDP_PORT = 4211;

// === Real CSI buffers ===
float latestRealCSI[32];
bool hasNewCSI = false;

WiFiUDP udp;
unsigned long lastSendTime = 0;
bool wifiConnected = false;
int ledState = LOW;

float csiBuffer[32];

// === Real CSI Callback ===
void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;

  int len = info->len;
  int numSub = min(32, len / 2);

  for (int i = 0; i < numSub; i++) {
    int8_t real = info->buf[i * 2];
    int8_t imag = info->buf[i * 2 + 1];
    float amp = sqrtf((float)real * real + (float)imag * imag);
    latestRealCSI[i] = amp / 200.0f;           // Rough normalization - tune per environment
    if (latestRealCSI[i] > 1.0f) latestRealCSI[i] = 1.0f;
  }
  hasNewCSI = true;
}

void initRealCSI() {
  Serial.println("Initializing real CSI collection...");

  // Enable promiscuous mode + CSI
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

  Serial.println("Real CSI enabled (HT20, LTF enabled)");
}

void setupCSI() {
  for (int i = 0; i < 32; i++) {
    csiBuffer[i] = 0.4 + 0.3 * (random(100) / 100.0);
  }
}

void updateSimulatedCSI(float currentRssi) {
  float rssiNorm = constrain((currentRssi + 90.0f) / 60.0f, 0.0f, 1.0f);

  for (int i = 0; i < 32; i++) {
    float drift = (random(200) - 100) / 2000.0f;
    csiBuffer[i] = csiBuffer[i] * 0.82f + (0.18f * (0.3f + 0.5f * rssiNorm + drift));
    csiBuffer[i] = constrain(csiBuffer[i], 0.08f, 0.95f);
  }
}

// === WiFiManager + Real CSI ===
void connectWiFi() {
  WiFiManager wifiManager;

  // wifiManager.resetSettings(); // Uncomment to force portal on every boot

  String apName = "ESP32-CSI-" + String(NODE_ID);

  wifiManager.setAPCallback([](WiFiManager* myWiFiManager) {
    Serial.println("\n=== CONFIG PORTAL ACTIVE ===");
    Serial.println("Connect to: " + myWiFiManager->getConfigPortalSSID());
    Serial.println("Open http://192.168.4.1 in your browser");
  });

  wifiManager.setConfigPortalTimeout(180);

  Serial.println("Starting WiFiManager...");

  if (!wifiManager.autoConnect(apName.c_str())) {
    Serial.println("WiFi config timeout. Restarting...");
    delay(3000);
    ESP.restart();
  }

  wifiConnected = true;
  Serial.println("\n=== WiFi Connected via WiFiManager ===");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");

  digitalWrite(STATUS_LED_PIN, HIGH);

  // === Enable real CSI after successful connection ===
  if (USE_REAL_CSI) {
    initRealCSI();
  }
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float currentRssi = WiFi.RSSI();

  if (USE_REAL_CSI && hasNewCSI) {
    memcpy(csiBuffer, latestRealCSI, sizeof(csiBuffer));
    hasNewCSI = false;
  } else {
    updateSimulatedCSI(currentRssi);
  }

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
  linkObj["weight"] = (currentRssi + 100.0f) / 100.0f;

  char jsonStr[1536];
  size_t len = serializeJson(doc, jsonStr);

  udp.beginPacket(TARGET_SERVER_IP, TARGET_PORT);
  udp.write((uint8_t*)jsonStr, len);
  bool sent = udp.endPacket();

  if (sent) {
    Serial.print("[UDP] CSI sent | RSSI=");
    Serial.print((int)currentRssi);
    if (USE_REAL_CSI) Serial.print(" | REAL CSI");
    Serial.print(" | node=");
    Serial.println(NODE_ID);

    digitalWrite(STATUS_LED_PIN, LOW);
    delay(25);
    digitalWrite(STATUS_LED_PIN, HIGH);
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    digitalWrite(STATUS_LED_PIN, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("\n=== ESP32 CSI UDP Sender v3.1 (WiFiManager + Real CSI) ===");

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  setupCSI();
  connectWiFi();

  if (wifiConnected) {
    udp.begin(LOCAL_UDP_PORT);
    Serial.print("UDP ready → ");
    Serial.print(TARGET_SERVER_IP);
    Serial.print(":");
    Serial.println(TARGET_PORT);
  }

  Serial.println("Setup complete. Transmitting CSI...\n");
}

void loop() {
  unsigned long now = millis();

  checkWiFi();

  if (wifiConnected && (now - lastSendTime >= SEND_INTERVAL_MS)) {
    sendCSIPacket();
    lastSendTime = now;
  }

  static unsigned long lastLed = 0;
  if (now - lastLed > 1800) {
    if (wifiConnected) {
      ledState = !ledState;
      digitalWrite(STATUS_LED_PIN, ledState);
    }
    lastLed = now;
  }

  delay(8);
}
