#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

// Canonical CSI → Echo Grid / wifi-sensing host
const uint16_t TARGET_PORT      = 4210;
const char* NODE_ID             = "esp32_csi_01";
const uint32_t SEND_INTERVAL_MS = 400;
const int STATUS_LED_PIN        = 2;
const bool USE_REAL_CSI         = true;
const uint16_t LOCAL_UDP_PORT   = 4211;

char targetServerIp[32] = "192.168.1.100";

float latestRealCSI[32];
bool hasNewCSI = false;
float csiBuffer[32];

WiFiUDP udp;
Preferences prefs;
unsigned long lastSendTime = 0;
bool wifiConnected = false;

void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf) return;
  int numSub = min(32, info->len / 2);
  for (int i = 0; i < numSub; i++) {
    int8_t real = info->buf[i * 2];
    int8_t imag = info->buf[i * 2 + 1];
    float amp = sqrtf((float)real * real + (float)imag * imag);
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
  Serial.println("[CSI] Real CSI enabled → UDP :4210");
}

void connectWiFi() {
  prefs.begin("csi", true);
  String saved = prefs.getString("server_ip", targetServerIp);
  prefs.end();
  saved.toCharArray(targetServerIp, sizeof(targetServerIp));

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);

  WiFiManagerParameter custom_server(
      "server_ip",
      "Echo Grid / CSI host IP",
      targetServerIp,
      31
  );
  wifiManager.addParameter(&custom_server);

  String apName = String("ESP32-CSI-") + NODE_ID;
  if (!wifiManager.autoConnect(apName.c_str())) {
    delay(2000);
    ESP.restart();
  }

  wifiConnected = true;
  const char* entered = custom_server.getValue();
  if (entered && strlen(entered) > 0) {
    strncpy(targetServerIp, entered, sizeof(targetServerIp) - 1);
    targetServerIp[sizeof(targetServerIp) - 1] = '\0';
    prefs.begin("csi", false);
    prefs.putString("server_ip", targetServerIp);
    prefs.end();
  }

  Serial.print("[WiFi] CSI target ");
  Serial.print(targetServerIp);
  Serial.println(":4210");

  if (USE_REAL_CSI) initRealCSI();
  digitalWrite(STATUS_LED_PIN, HIGH);
}

void sendCSIPacket() {
  if (!wifiConnected) return;

  float currentRssi = WiFi.RSSI();
  if (USE_REAL_CSI && hasNewCSI) {
    memcpy(csiBuffer, latestRealCSI, sizeof(csiBuffer));
    hasNewCSI = false;
  } else {
    for (int i = 0; i < 32; i++)
      csiBuffer[i] = constrain(0.3f + random(50) / 100.0f, 0.08f, 0.95f);
  }

  StaticJsonDocument<1536> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)currentRssi;
  doc["type"] = "wifi_csi";

  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(csiBuffer[i]);

  char jsonStr[1536];
  size_t len = serializeJson(doc, jsonStr);

  udp.beginPacket(targetServerIp, TARGET_PORT);
  udp.write((uint8_t*)jsonStr, len);
  udp.endPacket();

  Serial.printf("[UDP] → %s:%u | RSSI=%d\n", targetServerIp, TARGET_PORT, (int)currentRssi);
}

void setup() {
  Serial.begin(115200);
  delay(150);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  connectWiFi();
  if (wifiConnected) udp.begin(LOCAL_UDP_PORT);

  Serial.println("ESP32 CSI UDP sender ready (port 4210)");
}

void loop() {
  unsigned long now = millis();
  if (wifiConnected && (now - lastSendTime >= SEND_INTERVAL_MS)) {
    sendCSIPacket();
    lastSendTime = now;
  }
  delay(8);
}
