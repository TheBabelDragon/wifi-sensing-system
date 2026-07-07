#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// LoraW10 Gateway (with OLED)
// ESP-NOW + LoRa + GPS + Display
// ============================================================

// === PIN DEFINITIONS (adjust for your exact W10 board) ===
#define LORA_NSS    18
#define LORA_RST    23
#define LORA_DIO0   26
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27

#define GPS_RX      12   // GPS TX -> ESP32 RX
#define GPS_TX      15   // GPS RX -> ESP32 TX
#define GPS_BAUD    9600

// OLED I2C pins (common on W10-style boards)
#define OLED_SDA    21
#define OLED_SCL    22
#define OLED_ADDR   0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// === LORA SETTINGS ===
#define LORA_FREQUENCY  915E6
#define LORA_SYNC_WORD  0x12

// === CONFIG ===
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* UDP_TARGET_IP   = "192.168.1.100";
const uint16_t UDP_TARGET_PORT = 4210;

// === OBJECTS ===
WiFiUDP udp;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === ESP-NOW Receive ===
void onDataReceived(const uint8_t * mac, const uint8_t * data, int len) {
  Serial.printf("[ESP-NOW] Packet received (%d bytes)\n", len);

  // Forward via LoRa
  LoRa.beginPacket();
  LoRa.write(data, len);
  LoRa.endPacket();

  // Forward via UDP if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    udp.beginPacket(UDP_TARGET_IP, UDP_TARGET_PORT);
    udp.write(data, len);
    udp.endPacket();
  }

  // Show on OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP-NOW RX");
  display.setCursor(0, 16);
  display.printf("Len: %d bytes", len);
  display.display();

  Serial.write(data, len);
  Serial.println();
}

// === Gateway Heartbeat + GPS ===
void sendGatewayHeartbeat() {
  if (gps.location.isValid()) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"gw\":\"LoraW10\",\"lat\":%.6f,\"lon\":%.6f,\"sats\":%d}",
             gps.location.lat(), gps.location.lng(), gps.satellites.value());

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    // Show on display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Gateway Heartbeat");
    display.setCursor(0, 16);
    display.printf("Lat: %.4f", gps.location.lat());
    display.setCursor(0, 28);
    display.printf("Lon: %.4f", gps.location.lng());
    display.display();

    Serial.printf("[LoRa] Heartbeat: %s\n", payload);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // WiFi (optional)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
    udp.begin(4211);
  } else {
    Serial.println("\nLoRa-only mode (no WiFi)");
  }

  // GPS
  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED init failed");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("LoraW10 Gateway");
    display.display();
  }

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  Serial.println("LoRa OK");

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW failed");
    while (1);
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW Ready");

  Serial.println("=== LoraW10 Gateway Ready ===");
}

void loop() {
  // GPS
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  // Heartbeat every 30s
  static unsigned long lastHB = 0;
  if (millis() - lastHB > 30000) {
    sendGatewayHeartbeat();
    lastHB = millis();
  }

  delay(10);
}
