#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <cmath>

// ============================================================
// LoraW10 Gateway - Rich CSI + Orchestrator Features
// ============================================================

// === PIN DEFINITIONS ===
#define LORA_NSS     18
#define LORA_RST     23
#define LORA_DIO0    26
#define LORA_SCK      5
#define LORA_MISO    19
#define LORA_MOSI    27

#define GPS_RX       12
#define GPS_TX       15
#define GPS_BAUD   9600

#define OLED_SDA     21
#define OLED_SCL     22

#define LORA_FREQUENCY 915E6
#define LORA_SYNC_WORD 0x12

// === CONFIG ===
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* UDP_TARGET_IP   = "192.168.1.100";
const uint16_t UDP_TARGET_PORT = 4210;

// === OBJECTS ===
WiFiUDP udp;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// === CSI DATA (Rich Features) ===
float latestRealCSI[32];
float prevCSI[32];
bool hasNewCSI = false;

float bandMovement[4] = {0};
float bandVariance[4] = {0};
float movementIntensity = 0.0;
float nodeConfidence  = 0.6;

float csiVariance = 0;
float activityLevel = 0;
bool significantObstruction = false;
int hotZoneCount = 0;

int packetsReceived = 0;
String lastNode = "None";

// === CSI Callback with Band Awareness ===
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
  updateBandAwareCSIFeatures();
}

void updateBandAwareCSIFeatures() {
  for (int b = 0; b < 4; b++) {
    int start = b * 8;
    float mean = 0;
    float maxChange = 0;
    float var = 0;

    for (int i = 0; i < 8; i++) {
      int idx = start + i;
      mean += latestRealCSI[idx];
      float change = fabs(latestRealCSI[idx] - prevCSI[idx]);
      if (change > maxChange) maxChange = change;
    }
    mean /= 8.0f;

    for (int i = 0; i < 8; i++) {
      int idx = start + i;
      float diff = latestRealCSI[idx] - mean;
      var += diff * diff;
    }

    bandMovement[b] = maxChange;
    bandVariance[b] = var / 8.0f;
  }

  float meanAll = 0;
  for (int i = 0; i < 32; i++) meanAll += latestRealCSI[i];
  meanAll /= 32.0f;

  float varianceAll = 0;
  for (int i = 0; i < 32; i++) {
    float diff = latestRealCSI[i] - meanAll;
    varianceAll += diff * diff;
  }
  csiVariance = varianceAll / 32.0f;

  float strongestBand = 0;
  for (int b = 0; b < 4; b++) {
    if (bandMovement[b] > strongestBand) strongestBand = bandMovement[b];
  }

  movementIntensity = constrain(strongestBand * 4.8f + csiVariance * 1.6f, 0.0f, 1.0f);
  activityLevel = constrain(csiVariance * 6.2f + movementIntensity * 0.9f, 0.0f, 1.0f);
  hotZoneCount = constrain((int)(activityLevel * 6), 0, 6);
  significantObstruction = (hotZoneCount >= 3) || (activityLevel > 0.58f);

  static float prevMovement = 0;
  float consistency = 1.0f - fabs(movementIntensity - prevMovement);
  nodeConfidence = constrain(0.4f + consistency * 0.5f + (packetsReceived / 80.0f) * 0.3f, 0.35f, 0.92f);
  prevMovement = movementIntensity;

  for (int i = 0; i < 32; i++) prevCSI[i] = latestRealCSI[i];
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

  for (int i = 0; i < 32; i++) prevCSI[i] = 0.3f;
  Serial.println("[CSI] Rich Band-Aware CSI enabled on Gateway");
}

// === ESP-NOW Receive ===
void onDataReceived(const uint8_t * mac, const uint8_t * data, int len) {
  packetsReceived++;

  String payload((char*)data);
  int start = payload.indexOf("node");
  if (start != -1) {
    start += 7;
    int end = payload.indexOf('"', start);
    if (end > start) lastNode = payload.substring(start, end);
  }

  LoRa.beginPacket();
  LoRa.write(data, len);
  LoRa.endPacket();

  if (WiFi.status() == WL_CONNECTED) {
    udp.beginPacket(UDP_TARGET_IP, UDP_TARGET_PORT);
    udp.write(data, len);
    udp.endPacket();
  }
}

// === Display ===
void updateDisplay() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  u8g2.setCursor(0, 10);
  u8g2.print("LoraW10 Gateway");

  u8g2.setCursor(0, 25);
  u8g2.printf("Pkts:%d Act:%.2f", packetsReceived, activityLevel);

  u8g2.setCursor(0, 40);
  if (gps.location.isValid()) {
    u8g2.printf("GPS:%.4f", gps.location.lat());
  } else {
    u8g2.print("GPS: Searching");
  }

  u8g2.setCursor(0, 55);
  u8g2.printf("Mov:%.2f Conf:%.2f", movementIntensity, nodeConfidence);

  u8g2.sendBuffer();
}

// === Gateway Heartbeat ===
void sendGatewayHeartbeat() {
  if (gps.location.isValid()) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"gw\":\"LoraW10\",\"lat\":%.6f,\"lon\":%.6f}",
             gps.location.lat(), gps.location.lng());

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
  }
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("=== LoraW10 Gateway Starting ===");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    delay(200);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi connected: %s\n", WiFi.localIP().toString().c_str());
    udp.begin(4211);
  } else {
    Serial.println("Running in LoRa-only mode");
  }

  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS started");

  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("LoraW10 Gateway");
  u8g2.setCursor(0, 25);
  u8g2.print("Booting...");
  u8g2.sendBuffer();

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa FAILED");
    while (1);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  Serial.println("LoRa initialized");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW FAILED");
    while (1);
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW Ready");

  initRealCSI();

  Serial.println("=== LoraW10 Gateway (Rich CSI) Ready ===");
}

void loop() {
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  if (hasNewCSI) {
    hasNewCSI = false;
  }

  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 800) {
    updateDisplay();
    lastDisplay = millis();
  }

  static unsigned long lastHB = 0;
  if (millis() - lastHB > 30000) {
    sendGatewayHeartbeat();
    lastHB = millis();
  }

  delay(10);
}
