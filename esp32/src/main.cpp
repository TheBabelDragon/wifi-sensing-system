#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <cmath>

#ifndef HAS_DISPLAY
#define HAS_DISPLAY 0
#endif

#ifndef NODE_ID_STR
#define NODE_ID_STR "esp32_node_01"
#endif

#if HAS_DISPLAY
  #include <TFT_eSPI.h>
  TFT_eSPI tft = TFT_eSPI();
  #ifndef TFT_BL
    #define TFT_BL 21
  #endif
#endif

const uint16_t CSI_PORT         = 4210;
const uint16_t CMD_PORT         = 4211;
const char* NODE_ID             = NODE_ID_STR;
const int STATUS_LED_PIN        = 2;
const bool USE_REAL_CSI         = true;

// WiFi policy: sensing must NOT wait on host LAN join.
const uint32_t WIFI_CONNECT_TIMEOUT_MS = 12000;
const uint32_t WIFI_RECONNECT_MS       = 15000;
const uint32_t WIFI_PORTAL_TIMEOUT_S   = 90;

uint32_t sendIntervalMs         = 450;
uint32_t minIntervalMs          = 120;
uint32_t maxIntervalMs          = 1200;
float boostLevel                = 0.0f;

float fieldEntropy              = 0.0f;
int   fieldTracks               = 0;
float fieldMotion               = 0.0f;
float fieldDfMax                = 0.0f;
int   fieldNodes                = 0;
int   fieldBands                = 0;
bool  fieldAgreed               = false;
bool  hasFieldMirror            = false;

float latestRealCSI[32];
float prevCSI[32];
bool hasNewCSI = false;
bool csiHardwareOk = false;

float bandMovement[4] = {0};
float movementIntensity = 0.0;
float activityLevel = 0;
float nodeConfidence = 0.6;
int hotZoneCount = 0;
bool significantObstruction = false;
float csiVariance = 0;

WiFiUDP udpCsi;
WiFiUDP udpCmd;
unsigned long lastSendTime = 0;
unsigned long lastUiTime = 0;
unsigned long lastWifiAttempt = 0;
bool wifiConnected = false;
bool portalTried = false;
int packetCount = 0;
int cmdCount = 0;

const char* currentBandLabel() {
  int ch = WiFi.channel();
  if (ch >= 1 && ch <= 14) return "2.4";
  if (ch >= 36) return "5";
  return "2.4";
}

#if HAS_DISPLAY
const uint16_t COL_BG     = 0x0841;
const uint16_t COL_PANEL  = 0x1082;
const uint16_t COL_CYAN   = 0x07FF;
const uint16_t COL_MAG    = 0xF81F;
const uint16_t COL_ORANGE = 0xFD20;
const uint16_t COL_GREEN  = 0x07E0;
const uint16_t COL_RED    = 0xF800;
const uint16_t COL_YELLOW = 0xFFE0;
const uint16_t COL_WHITE  = 0xFFFF;
const uint16_t COL_DIM    = 0x8410;
const uint16_t COL_BAR_BG = 0x2104;
const uint16_t COL_PURPLE = 0xA01F;

void drawRoundPanel(int x, int y, int w, int h, uint16_t fill) {
  tft.fillRoundRect(x, y, w, h, 5, fill);
  tft.drawRoundRect(x, y, w, h, 5, COL_DIM);
}

void initDisplay() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COL_BG);

  tft.fillRect(0, 0, 320, 22, COL_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_CYAN, COL_PANEL);
  tft.drawString("ECHO GRID CSI", 4, 4, 2);
  tft.setTextColor(COL_WHITE, COL_PANEL);
  tft.drawString(NODE_ID, 130, 6, 1);
  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("CYD", 290, 6, 1);

  drawRoundPanel(2, 24, 156, 40, COL_PANEL);
  drawRoundPanel(162, 24, 76, 40, COL_PANEL);
  drawRoundPanel(242, 24, 76, 40, COL_PANEL);
  drawRoundPanel(2, 66, 316, 22, COL_PANEL);
  drawRoundPanel(2, 90, 316, 48, COL_PANEL);
  drawRoundPanel(2, 140, 316, 98, COL_PANEL);

  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("MOTION", 8, 26, 1);
  tft.drawString("BOOST", 168, 26, 1);
  tft.drawString("RATE", 248, 26, 1);
  tft.drawString("FIELD / FUSE", 8, 92, 1);
  tft.drawString("CSI SPECTRUM", 8, 142, 1);
}

void drawMotionBar(float v) {
  int x = 8, y = 38, w = 144, h = 18;
  tft.fillRoundRect(x, y, w, h, 3, COL_BAR_BG);
  int fill = (int)(v * (w - 2));
  if (fill < 0) fill = 0;
  if (fill > w - 2) fill = w - 2;
  uint16_t c = v > 0.7f ? COL_ORANGE : (v > 0.35f ? COL_CYAN : COL_GREEN);
  if (fill > 0) tft.fillRoundRect(x + 1, y + 1, fill, h - 2, 2, c);
  tft.setTextColor(COL_WHITE, COL_PANEL);
  char vb[8];
  snprintf(vb, sizeof(vb), "%.2f", v);
  tft.drawString(vb, 120, 26, 1);
}

void drawBoostGauge(float b) {
  tft.fillRect(168, 38, 64, 18, COL_BAR_BG);
  int fw = (int)(b * 62);
  if (fw > 0) tft.fillRect(169, 39, fw, 16, b > 0.3f ? COL_ORANGE : COL_DIM);
  tft.setTextColor(COL_WHITE, COL_PANEL);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)(b * 100));
  tft.drawString(buf, 168, 26, 1);
}

void drawRate() {
  tft.fillRect(248, 38, 64, 18, COL_BAR_BG);
  tft.setTextColor(COL_CYAN, COL_BAR_BG);
  char rb[12];
  snprintf(rb, sizeof(rb), "%ums", sendIntervalMs);
  tft.drawString(rb, 252, 42, 1);
}

void drawStatusChips() {
  tft.fillRect(6, 68, 308, 18, COL_PANEL);

  uint16_t wc = wifiConnected ? COL_GREEN : COL_RED;
  tft.fillRoundRect(6, 68, 48, 16, 3, wc);
  tft.setTextColor(COL_BG, wc);
  tft.drawString(wifiConnected ? "WiFi" : "OFF", 12, 71, 1);

  tft.fillRoundRect(58, 68, 56, 16, 3, COL_BAR_BG);
  tft.setTextColor(COL_YELLOW, COL_BAR_BG);
  char bb[12];
  snprintf(bb, sizeof(bb), "%s/%d", currentBandLabel(), WiFi.channel());
  tft.drawString(bb, 62, 71, 1);

  tft.fillRoundRect(118, 68, 52, 16, 3, COL_BAR_BG);
  tft.setTextColor(COL_MAG, COL_BAR_BG);
  char cb[12];
  snprintf(cb, sizeof(cb), "c%d", cmdCount);
  tft.drawString(cb, 124, 71, 1);

  tft.fillRoundRect(174, 68, 58, 16, 3, COL_BAR_BG);
  tft.setTextColor(COL_CYAN, COL_BAR_BG);
  char sb[12];
  snprintf(sb, sizeof(sb), "%ddBm", wifiConnected ? (int)WiFi.RSSI() : 0);
  tft.drawString(sb, 178, 71, 1);

  tft.fillRoundRect(236, 68, 40, 16, 3, COL_BAR_BG);
  tft.setTextColor(COL_WHITE, COL_BAR_BG);
  char pb[10];
  snprintf(pb, sizeof(pb), "p%d", packetCount % 1000);
  tft.drawString(pb, 240, 71, 1);

  uint16_t ac = (hasFieldMirror && fieldAgreed) ? COL_GREEN : COL_DIM;
  tft.fillRoundRect(280, 68, 34, 16, 3, ac);
  tft.setTextColor(COL_BG, ac);
  tft.drawString(hasFieldMirror && fieldAgreed ? "AGR" : "--", 284, 71, 1);
}

void drawFieldMirror() {
  tft.fillRect(6, 102, 308, 32, COL_PANEL);

  if (!wifiConnected) {
    tft.setTextColor(COL_YELLOW, COL_PANEL);
    tft.drawString("CSI live offline — joining LAN in background", 8, 110, 1);
    return;
  }
  if (!hasFieldMirror) {
    tft.setTextColor(COL_DIM, COL_PANEL);
    tft.drawString("waiting Echo Grid host on :4211...", 8, 110, 1);
    return;
  }

  tft.setTextColor(COL_CYAN, COL_PANEL);
  char l1[56];
  snprintf(l1, sizeof(l1), "H %.2f   T %d   |df| %.0f Hz",
           fieldEntropy, fieldTracks, fieldDfMax);
  tft.drawString(l1, 8, 102, 1);

  tft.setTextColor(COL_YELLOW, COL_PANEL);
  char l2[56];
  snprintf(l2, sizeof(l2), "nodes %d  bands %d  host-mot %.2f  %s",
           fieldNodes, fieldBands, fieldMotion,
           fieldAgreed ? "AGREED" : "single");
  tft.drawString(l2, 8, 116, 1);

  int bw = 50, bh = 10, bx = 262, by = 104;
  tft.fillRect(bx, by, bw, bh, COL_BAR_BG);
  int f = (int)(constrain(fieldEntropy * 2.0f, 0.0f, 1.0f) * (bw - 2));
  if (f > 0) tft.fillRect(bx + 1, by + 1, f, bh - 2,
                          fieldTracks > 0 ? COL_ORANGE : COL_CYAN);

  for (int i = 0; i < 6; i++) {
    uint16_t dc = (i < fieldTracks) ? COL_ORANGE : COL_BAR_BG;
    tft.fillCircle(268 + i * 8, 124, 3, dc);
  }
}

void drawSpectrum() {
  const int baseY = 230;
  const int maxH = 78;
  const int barW = 8;
  const int gap = 1;
  const int startX = 10;

  tft.fillRect(6, 154, 308, maxH + 6, COL_PANEL);

  for (int i = 0; i < 32; i++) {
    float v = latestRealCSI[i];
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    int h = (int)(v * maxH);
    if (h < 2) h = 2;
    int x = startX + i * (barW + gap);

    uint16_t c;
    if (fieldAgreed && hasFieldMirror) {
      c = v > 0.55f ? COL_ORANGE : COL_PURPLE;
    } else if (boostLevel > 0.4f || fieldTracks > 0) {
      c = v > 0.55f ? COL_ORANGE : COL_MAG;
    } else if (v > 0.65f) {
      c = COL_CYAN;
    } else if (v > 0.35f) {
      c = 0x5DFF;
    } else {
      c = 0x3A2F;
    }
    tft.fillRect(x, baseY - h, barW, h, c);
    tft.drawFastHLine(x, baseY - h, barW, COL_WHITE);
  }

  tft.setTextColor(COL_DIM, COL_PANEL);
  tft.drawString("0", 10, 232, 1);
  tft.drawString("16", 150, 232, 1);
  tft.drawString("31", 290, 232, 1);
}

void updateDisplay() {
  float mov = activityLevel > movementIntensity ? activityLevel : movementIntensity;
  drawMotionBar(mov);
  drawBoostGauge(boostLevel);
  drawRate();
  drawStatusChips();
  drawFieldMirror();
  drawSpectrum();
}
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
      int idx = b * 8 + i;
      float change = fabsf(latestRealCSI[idx] - prevCSI[idx]);
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
  for (int i = 0; i < 32; i++) prevCSI[i] = latestRealCSI[i];
}

void csi_rx_cb(void* ctx, wifi_csi_info_t* info) {
  if (!info || !info->buf || info->len < 2) return;
  int numSub = min(32, info->len / 2);
  for (int i = 0; i < numSub; i++) {
    int8_t re = info->buf[i * 2];
    int8_t im = info->buf[i * 2 + 1];
    float amp = sqrtf((float)re * re + (float)im * im) / 200.0f;
    latestRealCSI[i] = amp > 1.0f ? 1.0f : amp;
  }
  for (int i = numSub; i < 32; i++) latestRealCSI[i] = 0.0f;
  hasNewCSI = true;
  updateRichCSIFeatures();
}

bool initRealCSI() {
  // Radio stack must be up; association is NOT required.
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
  for (int i = 0; i < 32; i++) {
    latestRealCSI[i] = 0.3f;
    prevCSI[i] = 0.3f;
  }
  Serial.println("[CSI] hardware OK (independent of STA join)");
  return true;
}

bool trySavedWifi(uint32_t timeoutMs) {
  // Use credentials already stored by WiFiManager / NVS if present.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin();  // empty begin uses last saved STA config when available

  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st == WL_CONNECTED) {
      wifiConnected = true;
      statusLed(true);
      Serial.print("[WiFi] joined ");
      Serial.println(WiFi.localIP());
      return true;
    }
    delay(100);
  }
  wifiConnected = false;
  statusLed(false);
  Serial.println("[WiFi] no saved join yet — CSI stays live, retrying in background");
  return false;
}

void openConfigPortalOnce() {
  if (portalTried) return;
  portalTried = true;
  Serial.println("[WiFi] opening config portal (optional; CSI already running)");

  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
  wifiManager.setConnectTimeout(20);
  String apName = String("ESP32-CSI-") + NODE_ID;

  // Do not restart on failure — sensing must keep running.
  if (wifiManager.startConfigPortal(apName.c_str())) {
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiConnected) {
      statusLed(true);
      Serial.print("[WiFi] portal saved / joined ");
      Serial.println(WiFi.localIP());
    }
  } else {
    Serial.println("[WiFi] portal closed without join — continue offline CSI");
    wifiConnected = false;
  }
}

void maintainWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConnected) {
      wifiConnected = true;
      statusLed(true);
      Serial.print("[WiFi] (re)connected ");
      Serial.println(WiFi.localIP());
    }
    return;
  }

  if (wifiConnected) {
    wifiConnected = false;
    statusLed(false);
    Serial.println("[WiFi] link lost — CSI continues offline");
  }

  unsigned long now = millis();
  if (now - lastWifiAttempt < WIFI_RECONNECT_MS) return;
  lastWifiAttempt = now;

  // Background reconnect using saved credentials. Portal only once if never joined.
  if (!trySavedWifi(3000) && !portalTried) {
    // First boot with empty credentials: offer portal once, non-fatal.
    openConfigPortalOnce();
  }
}

void handleEchoCommand(const char* json, size_t len) {
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, json, len)) return;
  const char* type = doc["type"] | "";
  if (strcmp(type, "echo_cmd") != 0) return;
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
    uint32_t ms = doc["interval_ms"] | sendIntervalMs;
    sendIntervalMs = constrain(ms, minIntervalMs, maxIntervalMs);
  } else if (strcmp(cmd, "boost") == 0) {
    boostLevel = constrain((float)(doc["level"] | 0.7), 0.0f, 1.0f);
    sendIntervalMs = constrain((uint32_t)(450.0f - 280.0f * boostLevel), minIntervalMs, maxIntervalMs);
  } else if (strcmp(cmd, "quiet") == 0) {
    boostLevel = 0.0f;
    sendIntervalMs = 900;
  }
}

void pollCommands() {
  int packetSize = udpCmd.parsePacket();
  while (packetSize > 0) {
    char buf[512];
    int n = udpCmd.read(buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = 0;
      handleEchoCommand(buf, n);
    }
    packetSize = udpCmd.parsePacket();
  }
}

void sendCSIPacket() {
  // Always update local features / display path; only UDP needs link.
  float rssi = wifiConnected ? WiFi.RSSI() : -100.0f;
  if (!hasNewCSI) {
    for (int i = 0; i < 32; i++)
      latestRealCSI[i] = 0.35f + (random(30) / 100.0f);
    updateRichCSIFeatures();
  }

  if (!wifiConnected) {
    hasNewCSI = false;
    return;
  }

  StaticJsonDocument<1600> doc;
  doc["node"] = NODE_ID;
  doc["timestamp"] = millis();
  doc["rssi"] = (int)rssi;
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
  doc["cmd_count"] = cmdCount;

  JsonArray bandMov = doc.createNestedArray("band_movement");
  for (int b = 0; b < 4; b++) bandMov.add(bandMovement[b]);
  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);

  char buf[1600];
  size_t n = serializeJson(doc, buf);

  IPAddress bcast = WiFi.localIP();
  bcast[3] = 255;
  if (udpCsi.beginPacket(bcast, CSI_PORT)) {
    udpCsi.write((uint8_t*)buf, n);
    udpCsi.endPacket();
  }
  if (udpCsi.beginPacket(IPAddress(255, 255, 255, 255), CSI_PORT)) {
    udpCsi.write((uint8_t*)buf, n);
    udpCsi.endPacket();
  }

  packetCount++;
  hasNewCSI = false;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("=== ESP32 CSI (sensing first, WiFi second) ===");
  Serial.printf("NODE_ID=%s  CSI:%u  CMD:%u  DISPLAY=%d\n",
                NODE_ID, CSI_PORT, CMD_PORT, HAS_DISPLAY);

#if HAS_DISPLAY
  initDisplay();
#else
  pinMode(STATUS_LED_PIN, OUTPUT);
  statusLed(false);
#endif

  // 1) Radio stack up
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  delay(50);

  // 2) CSI immediately — does not wait for host LAN
  if (USE_REAL_CSI) {
    csiHardwareOk = initRealCSI();
    if (!csiHardwareOk) Serial.println("[CSI] soft fallback");
  }

  // 3) UDP sockets ready even before join (binds local ports)
  udpCsi.begin(4212);
  udpCmd.begin(CMD_PORT);

  // 4) Best-effort quick join; never blocks sensing forever
  lastWifiAttempt = millis();
  trySavedWifi(WIFI_CONNECT_TIMEOUT_MS);

#if HAS_DISPLAY
  updateDisplay();
#endif
  Serial.println("[boot] CSI path live; WiFi maintained in background");
}

void loop() {
  maintainWifi();
  pollCommands();

  if (millis() - lastSendTime >= sendIntervalMs) {
    sendCSIPacket();
    lastSendTime = millis();
  }
#if HAS_DISPLAY
  if (millis() - lastUiTime > 200) {
    updateDisplay();
    lastUiTime = millis();
  }
#endif
  delay(5);
}
