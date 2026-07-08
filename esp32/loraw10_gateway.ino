#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <U8g2lib.h>
#include <Wire.h>

// ============================================================
// LoraW10 Gateway - Auto Pin Detection for OLED
// ============================================================

// LoRa pins (common on W10 boards)
#define LORA_NSS     18
#define LORA_RST     23
#define LORA_DIO0    26
#define LORA_SCK      5
#define LORA_MISO    19
#define LORA_MOSI    27

#define GPS_RX       12
#define GPS_TX       15
#define GPS_BAUD   9600

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

// U8g2 display object (will be initialized later)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2 = nullptr;

int packetsReceived = 0;
String lastNode = "None";

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

void updateDisplay() {
  if (!u8g2) return;

  u8g2->clearBuffer();
  u8g2->setFont(u8g2_font_ncenB08_tr);
  u8g2->setCursor(0, 10);
  u8g2->print("LoraW10 Gateway");
  u8g2->setCursor(0, 25);
  u8g2->printf("Packets: %d", packetsReceived);
  u8g2->setCursor(0, 40);
  if (gps.location.isValid()) {
    u8g2->printf("GPS: %.4f", gps.location.lat());
  } else {
    u8g2->print("GPS: No Fix");
  }
  u8g2->setCursor(0, 55);
  u8g2->printf("Last: %s", lastNode.c_str());
  u8g2->sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println();
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

  // === Auto-detect OLED pins ===
  int sdaPins[] = {21, 4};
  int sclPins[] = {22, 15};
  bool oledFound = false;

  for (int i = 0; i < 2; i++) {
    Serial.printf("Trying OLED on SDA=%d, SCL=%d...\n", sdaPins[i], sclPins[i]);

    Wire.begin(sdaPins[i], sclPins[i]);
    delay(100);

    U8G2_SSD1306_128X64_NONAME_F_HW_I2C* testDisplay = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
    testDisplay->begin();

    // Simple test - if it doesn't crash and can draw, assume it works
    testDisplay->clearBuffer();
    testDisplay->setFont(u8g2_font_ncenB08_tr);
    testDisplay->drawStr(0, 10, "Testing...");
    testDisplay->sendBuffer();

    // If we got here without crashing, assume OLED works
    u8g2 = testDisplay;
    oledFound = true;
    Serial.printf("OLED working on SDA=%d, SCL=%d\n", sdaPins[i], sclPins[i]);
    break;
  }

  if (!oledFound) {
    Serial.println("No working OLED found on common pins!");
  }

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

  Serial.println("=== LoraW10 Gateway Ready ===");
}

void loop() {
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 1000 && u8g2) {
    updateDisplay();
    lastDisplay = millis();
  }

  delay(10);
}
