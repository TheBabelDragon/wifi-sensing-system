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
// LoraW10 Gateway - Full CSI + OLED + ESP-NOW + LoRa + GPS
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

// === CSI DATA ===
float latestRealCSI[32];
bool hasNewCSI = false;
float csiVariance = 0;
float activityLevel = 0;
int hotZoneCount = 0;
int packetsReceived = 0;
String lastNode = "None";

// === CSI Callback ===
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
    .manu_scale = false
  };

  esp_wifi_set_csi_config(&csi_config);
  esp_wifi_set_csi_rx_cb(csi_rx_cb, NULL);
  esp_wifi_set_csi(true);

  Serial.println("[CSI] Real CSI collection enabled");
}

// === Update Metrics ===
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

  activityLevel = constrain(csiVariance * 7.0f, 0.0f, 1.0f);
  hotZoneCount = constrain((int)(activityLevel * 5), 0, 5);
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

  // Forward via LoRa
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
  u8g2.printf("Packets: %d  Act: %.2f", packetsReceived, activityLevel);

  u8g2.setCursor(0, 40);
  if (gps.location.isValid()) {
    u8g2.printf("GPS: %.4f", gps.location.lat());
  } else {
    u8g2.print("GPS: Searching");
  }

  u8g2.setCursor(0, 55);
  u8g2.printf("HotZones: %d  Last: %s", hotZoneCount, lastNode.c_str());

  u8g2.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("=== LoraW10 Gateway Starting ===");

  // WiFi
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

  // GPS
  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS started");

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(0, 10);
  u8g2.print("LoraW10 Gateway");
  u8g2.setCursor(0, 25);
  u8g2.print("Booting...");
  u8g2.sendBuffer();
  Serial.println("OLED initialized");

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa FAILED");
    while (1);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  Serial.println("LoRa initialized");

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW FAILED");
    while (1);
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW Ready");

  // CSI
  initRealCSI();

  Serial.println("=== LoraW10 Gateway Fully Ready ===");
}

void loop() {
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  if (hasNewCSI) {
    updateCSIMetrics();
    hasNewCSI = false;
  }

  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 800) {
    updateDisplay();
    lastDisplay = millis();
  }

  delay(10);
}
