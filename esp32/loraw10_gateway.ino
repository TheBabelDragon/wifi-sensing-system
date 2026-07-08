#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// LoraW10 Gateway - Robust OLED Version
// ============================================================

// === CHANGE THESE IF YOUR BOARD IS DIFFERENT ===
#define LORA_NSS     18
#define LORA_RST     23
#define LORA_DIO0    26
#define LORA_SCK      5
#define LORA_MISO    19
#define LORA_MOSI    27

#define GPS_RX       12
#define GPS_TX       15
#define GPS_BAUD   9600

// OLED pins (most common on W10 boards)
#define OLED_SDA     21
#define OLED_SCL     22
#define OLED_RESET   -1      // Set to a GPIO if your board has OLED RST pin
#define OLED_ADDR    0x3C    // Try 0x3D if this doesn't work

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int packetsReceived = 0;
String lastNode = "None";
float lastActivity = 0;
int lastHotZones = 0;
unsigned long lastDisplayUpdate = 0;
int displayPage = 0;

// === ESP-NOW ===
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

  lastDisplayUpdate = millis();
  displayPage = 1;
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (displayPage == 0) {
    display.setCursor(0, 0);
    display.println("LoraW10 Gateway");
    display.setCursor(0, 16);
    display.printf("Packets: %d", packetsReceived);
    display.setCursor(0, 28);
    if (gps.location.isValid()) {
      display.printf("GPS: %.4f", gps.location.lat());
    } else {
      display.println("GPS: No Fix");
    }
    display.setCursor(0, 40);
    display.printf("Last: %s", lastNode.c_str());

  } else if (displayPage == 1) {
    display.setCursor(0, 0);
    display.println("Last Node");
    display.setCursor(0, 16);
    display.print(lastNode);
    display.setCursor(0, 32);
    display.printf("Act: %.2f  HZ: %d", lastActivity, lastHotZones);

    if (millis() - lastDisplayUpdate > 4000) displayPage = 0;
  }

  display.display();
}

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
  delay(300);

  Serial.println("=== LoraW10 Gateway Booting ===");

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
    Serial.println("Running without WiFi");
  }

  // GPS
  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  // OLED - More robust initialization
  Wire.begin(OLED_SDA, OLED_SCL);
  delay(100);

  bool displayOK = false;
  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    displayOK = true;
    Serial.println("OLED OK at 0x3C");
  } else if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
    displayOK = true;
    Serial.println("OLED OK at 0x3D");
  }

  if (displayOK) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("LoraW10 Gateway");
    display.setCursor(0, 20);
    display.println("Starting...");
    display.display();
    delay(600);
  } else {
    Serial.println("!!! OLED NOT FOUND !!!");
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
  while (GPS_Serial.available()) gps.encode(GPS_Serial.read());

  if (millis() - lastDisplayUpdate > 1000) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  static unsigned long lastHB = 0;
  if (millis() - lastHB > 30000) {
    sendGatewayHeartbeat();
    lastHB = millis();
  }

  delay(10);
}
