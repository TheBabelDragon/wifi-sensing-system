#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_now.h>
#include <cmath>
#include <cstring>

#ifndef HAS_DISPLAY
#define HAS_DISPLAY 0
#endif

#if HAS_DISPLAY
  #include <TFT_eSPI.h>
  TFT_eSPI tft = TFT_eSPI();
  #ifndef TFT_BL
    #define TFT_BL 21
  #endif
#endif

const uint16_t CSI_PORT = 4210;
const uint16_t CMD_PORT = 4211;
const int STATUS_LED_PIN = 2;
const bool USE_REAL_CSI = true;
const uint8_t OFFLINE_CSI_CHANNEL = 6;

const uint32_t WIFI_CONNECT_TIMEOUT_MS = 8000;
const uint32_t WIFI_RECONNECT_MS       = 30000;
const uint32_t WIFI_PORTAL_TIMEOUT_S   = 180;

// Runtime unique ID from MAC — never "node_01"
// Examples: csi-A1B2C3  /  cyd-A1B2C3
char NODE_ID[16] = "csi-boot";

uint32_t sendIntervalMs = 500;
uint32_t minIntervalMs  = 150;
uint32_t maxIntervalMs  = 1200;
float boostLevel = 0.0f;

float fieldEntropy = 0, fieldMotion = 0, fieldDfMax = 0;
int fieldTracks = 0, fieldNodes = 0, fieldBands = 0;
bool fieldAgreed = false, hasFieldMirror = false;

float csiScratch[32];
float latestRealCSI[32];
float prevCSI[32];
volatile bool hasNewCSI = false;
bool csiHardwareOk = false;
volatile uint32_t csiIrqCount = 0;

float bandMovement[4] = {0};
float movementIntensity = 0, activityLevel = 0, nodeConfidence = 0.6f;
int hotZoneCount = 0;
bool significantObstruction = false;
float csiVariance = 0;

WiFiUDP udpCsi;
WiFiUDP udpCmd;
unsigned long lastSendTime = 0;
unsigned long lastUiTime = 0;
unsigned long lastWifiAttempt = 0;
bool wifiConnected = false;
bool hasStaCredentials = false;
int packetCount = 0, cmdCount = 0;
int espNowTxCount = 0, espNowRxCount = 0;
bool espNowOk = false;
bool espNowEverInited = false;
bool displayOk = false;

char serialBuf[48];
size_t serialLen = 0;

#pragma pack(push, 1)
struct EspNowCsiPkt {
  char     magic[4];
  char     node[12];
  uint32_t ts_ms;
  int8_t   rssi;
  uint8_t  channel;
  uint8_t  activity;
  uint8_t  movement;
  uint8_t  hot_zones;
  uint8_t  flags;
  uint8_t  csi[32];
};
#pragma pack(pop)

static const uint8_t ESPNOW_BROADCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
volatile bool espNowRxPending = false;
EspNowCsiPkt  espNowRxPkt;

// Build unique node id from chip MAC (last 3 octets)
void makeNodeId() {
#ifdef NODE_ID_STR
  // Optional compile-time override
  strncpy(NODE_ID, NODE_ID_STR, sizeof(NODE_ID) - 1);
  NODE_ID[sizeof(NODE_ID) - 1] = 0;
#else
  uint8_t mac[6];
  WiFi.macAddress(mac);
#if HAS_DISPLAY
  snprintf(NODE_ID, sizeof(NODE_ID), "cyd-%02X%02X%02X", mac[3], mac[4], mac[5]);
#else
  snprintf(NODE_ID, sizeof(NODE_ID), "csi-%02X%02X%02X", mac[3], mac[4], mac[5]);
#endif
#endif
}

const char* currentBandLabel() {
  int ch = WiFi.channel();
  return (ch >= 36) ? "5" : "2.4";
}

bool staCredentialsSaved() {
  wifi_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) return false;
  return cfg.sta.ssid[0] != 0;
}

void lockOfflineChannel() {
  esp_wifi_set_channel(OFFLINE_CSI_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

#if HAS_DISPLAY
const uint16_t COL_BG = 0x0841, COL_PANEL = 0x1082, COL_CYAN = 0x07FF;
const uint16_t COL_GREEN = 0x07E0, COL_YELLOW = 0xFFE0, COL_WHITE = 0xFFFF;
const uint16_t COL_DIM = 0x8410, COL_BAR_BG = 0x2104;

void initDisplay() {
  // CYD backlight — never block boot if panel fails
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  delay(20);

  tft.init();
  tft.setRotation(1);          // landscape 320x240
  tft.fillScreen(COL_BG);

  tft.fillRect(0, 0, 320, 24, COL_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_CYAN, COL_PANEL);
  tft.drawString("ECHO GRID", 6, 4, 2);
  tft.setTextColor(COL_WHITE, COL_PANEL);
  tft.drawString(NODE_ID, 120, 6, 2);

  tft.setTextColor(COL_DIM, COL_BG);
  tft.drawString("booting...", 6, 40, 2);
  displayOk = true;
  Serial.println("[TFT] OK");
}

void updateDisplay() {
  if (!displayOk) return;
  tft.fillRect(0, 30, 320, 30, COL_BG);

  uint16_t wc = wifiConnected ? COL_GREEN : COL_YELLOW;
  tft.fillRoundRect(6, 34, 56, 20, 3, wc);
  tft.setTextColor(COL_BG, wc);
  tft.drawString(wifiConnected ? "WiFi" : "LOC", 14, 38, 2);

  tft.fillRoundRect(70, 34, 90, 20, 3, COL_BAR_BG);
  tft.setTextColor(COL_YELLOW, COL_BAR_BG);
  char line[24];
  snprintf(line, sizeof(line), "ch %d", WiFi.channel());
  tft.drawString(line, 78, 38, 2);

  tft.fillRoundRect(170, 34, 140, 20, 3, COL_BAR_BG);
  tft.setTextColor(COL_CYAN, COL_BAR_BG);
  snprintf(line, sizeof(line), "EN %d", espNowTxCount);
  tft.drawString(line, 178, 38, 2);
}
#else
void initDisplay() {}
void updateDisplay() {}
#endif

void statusLed(bool on) {
#if !HAS_DISPLAY
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#else
  (void)on;
#endif
}

void updateRichCSIFeatures() {
  for (int b = 0; b < 4; b++) {
    float maxChange = 0;
    for (int i = 0; i < 8; i++) {
      float change = fabsf(latestRealCSI[b * 8 + i] - prevCSI[b * 8 + i]);
      if (change > maxChange) maxChange = change;
    }
    bandMovement[b] = maxChange;
  }
  float meanAll = 0;
  for (int i = 0; i < 32; i++) meanAll += latestRealCSI[i];
  meanAll /= 32.0f;
  float varianceAll = 0;
  for (int i = 0; i < 32; i++) {
    float d = latestRealCSI[i] - meanAll;
    varianceAll += d * d;
  }
  csiVariance = varianceAll / 32.0f;
  float strongest = 0;
  for (int b = 0; b < 4; b++)
    if (bandMovement[b] > strongest) strongest = bandMovement[b];
  float gain = 1.0f + 0.8f * boostLevel;
  movementIntensity = constrain((strongest * 4.8f + csiVariance * 1.6f) * gain, 0.0f, 1.0f);
  activityLevel = constrain((csiVariance * 6.2f + movementIntensity * 0.9f) * gain, 0.0f, 1.0f);
  hotZoneCount = constrain((int)(activityLevel * 6), 0, 6);
  significantObstruction = (hotZoneCount >= 3) || (activityLevel > 0.58f);
  nodeConfidence = constrain(0.4f + activityLevel * 0.5f, 0.35f, 0.92f);
  memcpy(prevCSI, latestRealCSI, sizeof(prevCSI));
}

// Keep ISR light — no sqrt in IRAM path
void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf || info->len < 2) return;
  int numSub = info->len / 2;
  if (numSub > 32) numSub = 32;
  for (int i = 0; i < numSub; i++) {
    int8_t re = info->buf[i * 2];
    int8_t im = info->buf[i * 2 + 1];
    // Manhattan magnitude approx — fast, good enough for activity
    int mag = (re < 0 ? -re : re) + (im < 0 ? -im : im);
    float amp = mag / 280.0f;
    csiScratch[i] = amp > 1.0f ? 1.0f : amp;
  }
  for (int i = numSub; i < 32; i++) csiScratch[i] = 0.0f;
  hasNewCSI = true;
  csiIrqCount++;
}

void consumeNewCsi() {
  if (!hasNewCSI) return;
  noInterrupts();
  memcpy(latestRealCSI, csiScratch, sizeof(latestRealCSI));
  hasNewCSI = false;
  interrupts();
  updateRichCSIFeatures();
}

bool initRealCSI() {
  if (esp_wifi_set_promiscuous(true) != ESP_OK) return false;
  wifi_csi_config_t cfg = {};
  cfg.lltf_en = true;
  cfg.htltf_en = true;
  cfg.stbc_htltf2_en = true;
  cfg.ltf_merge_en = true;
  cfg.manu_scale = false;
  if (esp_wifi_set_csi_config(&cfg) != ESP_OK) return false;
  if (esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL) != ESP_OK) return false;
  if (esp_wifi_set_csi(true) != ESP_OK) return false;
  for (int i = 0; i < 32; i++)
    latestRealCSI[i] = prevCSI[i] = csiScratch[i] = 0.3f;
  return true;
}

void onEspNowSent(const uint8_t *mac, esp_now_send_status_t status) {
  (void)mac; (void)status;
}

void onEspNowRecv(const uint8_t *mac, const uint8_t *data, int len) {
  (void)mac;
  if (len < (int)sizeof(EspNowCsiPkt)) return;
  const EspNowCsiPkt* pkt = (const EspNowCsiPkt*)data;
  if (memcmp(pkt->magic, "CSI1", 4) != 0) return;
  if (strncmp(pkt->node, NODE_ID, sizeof(pkt->node)) == 0) return;
  memcpy(&espNowRxPkt, pkt, sizeof(EspNowCsiPkt));
  espNowRxPending = true;
  espNowRxCount++;
}

bool initEspNow() {
  if (espNowEverInited) {
    esp_now_deinit();
    delay(10);
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] init failed");
    espNowOk = false;
    return false;
  }
  espNowEverInited = true;
  esp_now_register_send_cb(onEspNowSent);
  esp_now_register_recv_cb(onEspNowRecv);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, ESPNOW_BROADCAST, 6);
  peer.channel = 0;
  peer.encrypt = false;
  peer.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("[ESP-NOW] peer failed");
    espNowOk = false;
    return false;
  }
  espNowOk = true;
  Serial.println("[ESP-NOW] ready");
  return true;
}

void sendEspNowCsi() {
  if (!espNowOk) return;
  EspNowCsiPkt pkt = {};
  memcpy(pkt.magic, "CSI1", 4);
  strncpy(pkt.node, NODE_ID, sizeof(pkt.node) - 1);
  pkt.ts_ms     = millis();
  pkt.rssi      = wifiConnected ? (int8_t)WiFi.RSSI() : (int8_t)-100;
  pkt.channel   = (uint8_t)WiFi.channel();
  pkt.activity  = (uint8_t)constrain((int)(activityLevel * 255.0f), 0, 255);
  pkt.movement  = (uint8_t)constrain((int)(movementIntensity * 255.0f), 0, 255);
  pkt.hot_zones = (uint8_t)hotZoneCount;
  pkt.flags     = significantObstruction ? 0x01 : 0x00;
  for (int i = 0; i < 32; i++) {
    float v = latestRealCSI[i];
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    pkt.csi[i] = (uint8_t)(v * 255.0f);
  }
  if (esp_now_send(ESPNOW_BROADCAST, (uint8_t*)&pkt, sizeof(pkt)) == ESP_OK)
    espNowTxCount++;
}

void forwardEspNowToUdp(const EspNowCsiPkt* pkt) {
  if (!wifiConnected || !pkt) return;
  StaticJsonDocument<640> doc;
  doc["node"] = pkt->node;
  doc["timestamp"] = pkt->ts_ms;
  doc["rssi"] = (int)pkt->rssi;
  doc["type"] = "wifi_csi";
  doc["channel"] = pkt->channel;
  doc["activity"] = pkt->activity / 255.0f;
  doc["movement_intensity"] = pkt->movement / 255.0f;
  doc["hot_zones"] = pkt->hot_zones;
  doc["obstruction"] = (pkt->flags & 0x01) != 0;
  doc["via"] = "espnow";
  doc["bridge"] = NODE_ID;
  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(pkt->csi[i] / 255.0f);
  char buf[640];
  size_t n = serializeJson(doc, buf);
  IPAddress bcast = WiFi.localIP();
  bcast[3] = 255;
  if (udpCsi.beginPacket(bcast, CSI_PORT)) {
    udpCsi.write((uint8_t*)buf, n);
    udpCsi.endPacket();
  }
}

void processEspNowRx() {
  if (!espNowRxPending) return;
  EspNowCsiPkt local;
  noInterrupts();
  memcpy(&local, &espNowRxPkt, sizeof(local));
  espNowRxPending = false;
  interrupts();
  forwardEspNowToUdp(&local);
}

bool trySavedWifi(uint32_t timeoutMs) {
  if (!hasStaCredentials) {
    lockOfflineChannel();
    wifiConnected = false;
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      statusLed(true);
      Serial.print("[WiFi] joined ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(40);
    yield();
  }
  wifiConnected = false;
  statusLed(false);
  lockOfflineChannel();
  return false;
}

// Portal AP name is unique per board: CSI-cyd-A1B2C3
void runWifiSetup() {
  hasStaCredentials = staCredentialsSaved();

  // Fast path: already provisioned → try join, skip portal if OK
  if (hasStaCredentials) {
    Serial.println("[WiFi] trying saved network...");
    if (trySavedWifi(WIFI_CONNECT_TIMEOUT_MS)) {
      Serial.println("[WiFi] saved network OK — no portal");
      return;
    }
    Serial.println("[WiFi] saved network failed — opening portal");
  }

  char apName[24];
  snprintf(apName, sizeof(apName), "CSI-%s", NODE_ID);

  Serial.println();
  Serial.println("========================================");
  Serial.println("  WiFiManager portal");
  Serial.printf( "  Join AP:  %s\n", apName);
  Serial.println("  Set house WiFi on your phone");
  Serial.println("  Timeout 3 minutes");
  Serial.println("========================================");
  Serial.flush();

#if HAS_DISPLAY
  if (displayOk) {
    tft.fillScreen(0x0841);
    tft.setTextColor(0x07FF, 0x0841);
    tft.drawString("WiFi setup", 6, 20, 2);
    tft.setTextColor(0xFFFF, 0x0841);
    tft.drawString(apName, 6, 50, 2);
    tft.setTextColor(0x8410, 0x0841);
    tft.drawString("Join AP on phone", 6, 90, 2);
  }
#endif

  WiFi.mode(WIFI_STA);
  delay(30);

  WiFiManager wm;
  wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(15);
  wm.setDebugOutput(false);

  bool ok = wm.startConfigPortal(apName);
  hasStaCredentials = staCredentialsSaved();

  if (ok && WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    statusLed(true);
    Serial.print("[WiFi] connected ");
    Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    statusLed(false);
    WiFi.mode(WIFI_STA);
    lockOfflineChannel();
    Serial.println("[WiFi] portal done — offline ESP-NOW mode");
  }
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      statusLed(true);
      Serial.print("[WiFi] (re)connected ");
      Serial.println(WiFi.localIP());
      initEspNow();
    }
    return;
  }
  if (wifiConnected) {
    wifiConnected = false;
    statusLed(false);
    lockOfflineChannel();
    initEspNow();
    Serial.println("[WiFi] lost — local ESP-NOW");
  }
  if (!hasStaCredentials) return;
  unsigned long now = millis();
  if (now - lastWifiAttempt < WIFI_RECONNECT_MS) return;
  lastWifiAttempt = now;
  trySavedWifi(5000);
}

void printStatus() {
  Serial.printf(
    "[status] id=%s wifi=%d creds=%d csi=%d espnow=%d ch=%d udp=%d en_tx=%d en_rx=%d\n",
    NODE_ID, wifiConnected, hasStaCredentials, csiHardwareOk, espNowOk,
    WiFi.channel(), packetCount, espNowTxCount, espNowRxCount
  );
}

void handleSerialLine(const char* line) {
  if (strcasecmp(line, "status") == 0) printStatus();
  else if (strcasecmp(line, "portal") == 0) runWifiSetup();
  else if (line[0]) Serial.println("[serial] status | portal");
}

void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serialBuf[serialLen] = 0;
      if (serialLen > 0) handleSerialLine(serialBuf);
      serialLen = 0;
    } else if (serialLen + 1 < sizeof(serialBuf)) {
      serialBuf[serialLen++] = c;
    } else {
      serialLen = 0;
    }
  }
}

void handleEchoCommand(const char* json, size_t len) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json, len)) return;
  if (strcmp(doc["type"] | "", "echo_cmd") != 0) return;
  const char* cmd = doc["cmd"] | "";
  cmdCount++;
  if (strcmp(cmd, "field") == 0) {
    fieldEntropy = doc["entropy"] | 0.0;
    fieldTracks  = doc["tracks"] | 0;
    fieldMotion  = doc["motion"] | 0.0;
    fieldDfMax   = doc["df_max"] | 0.0;
    fieldNodes   = doc["nodes"] | 0;
    fieldBands   = doc["bands"] | 0;
    fieldAgreed  = doc["agreed"] | false;
    hasFieldMirror = true;
  } else if (strcmp(cmd, "set_rate") == 0) {
    sendIntervalMs = constrain((uint32_t)(doc["interval_ms"] | sendIntervalMs),
                               minIntervalMs, maxIntervalMs);
  } else if (strcmp(cmd, "boost") == 0) {
    boostLevel = constrain((float)(doc["level"] | 0.7), 0.0f, 1.0f);
    sendIntervalMs = constrain((uint32_t)(450.0f - 280.0f * boostLevel),
                               minIntervalMs, maxIntervalMs);
  } else if (strcmp(cmd, "quiet") == 0) {
    boostLevel = 0;
    sendIntervalMs = 900;
  }
}

void pollCommands() {
  int packetSize = udpCmd.parsePacket();
  while (packetSize > 0) {
    char buf[512];
    int n = udpCmd.read(buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = 0; handleEchoCommand(buf, n); }
    packetSize = udpCmd.parsePacket();
  }
}

void sendUdpCsi() {
  if (!wifiConnected) return;
  StaticJsonDocument<1400> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)WiFi.RSSI();
  doc["type"] = "wifi_csi";
  doc["band"] = currentBandLabel();
  doc["channel"] = WiFi.channel();
  doc["activity"] = activityLevel;
  doc["hot_zones"] = hotZoneCount;
  doc["obstruction"] = significantObstruction;
  doc["movement_intensity"] = movementIntensity;
  doc["confidence"] = nodeConfidence;
  doc["interval_ms"] = sendIntervalMs;
  doc["boost"] = boostLevel;
  doc["via"] = "udp";
  JsonArray bandMov = doc.createNestedArray("band_movement");
  for (int b = 0; b < 4; b++) bandMov.add(bandMovement[b]);
  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);
  char buf[1400];
  size_t n = serializeJson(doc, buf);
  IPAddress bcast = WiFi.localIP();
  bcast[3] = 255;
  if (udpCsi.beginPacket(bcast, CSI_PORT)) {
    udpCsi.write((uint8_t*)buf, n);
    udpCsi.endPacket();
  }
  packetCount++;
}

void sendCSIPacket() {
  consumeNewCsi();
  if (csiIrqCount == 0) {
    for (int i = 0; i < 32; i++)
      latestRealCSI[i] = 0.35f + (random(30) / 100.0f);
    updateRichCSIFeatures();
  }
  sendEspNowCsi();
  sendUdpCsi();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== ESP32 CSI + ESP-NOW ===");
  Serial.flush();

#if !HAS_DISPLAY
  pinMode(STATUS_LED_PIN, OUTPUT);
  statusLed(false);
#endif

  // WiFi stack first so we can read MAC for unique ID
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  delay(50);
  makeNodeId();
  Serial.printf("NODE_ID=%s  (unique per board)\n", NODE_ID);
  Serial.flush();

  // Display after we know NODE_ID (CYD only)
#if HAS_DISPLAY
  initDisplay();
#endif

  // WiFi setup BEFORE CSI/ESP-NOW — clean radio for portal
  runWifiSetup();

  // Sensing path after portal
  if (USE_REAL_CSI) {
    csiHardwareOk = initRealCSI();
    Serial.println(csiHardwareOk ? "[CSI] OK" : "[CSI] fallback");
  }
  if (WiFi.status() != WL_CONNECTED) lockOfflineChannel();
  initEspNow();

  udpCsi.begin(4212);
  udpCmd.begin(CMD_PORT);

  lastWifiAttempt = millis();
  lastSendTime = millis();

  Serial.println("[boot] running");
  printStatus();
  Serial.flush();
}

void loop() {
  pollSerial();
  processEspNowRx();
  maintainWifi();
  pollCommands();

  unsigned long now = millis();
  if (now - lastSendTime >= sendIntervalMs) {
    sendCSIPacket();
    lastSendTime = now;
  }

#if HAS_DISPLAY
  if (displayOk && now - lastUiTime > 500) {
    updateDisplay();
    lastUiTime = now;
  }
#endif

  delay(3);
  yield();
}
