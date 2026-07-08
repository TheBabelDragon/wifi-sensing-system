#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// LoraW10 Gateway (with OLED) - Enhanced Display
// ============================================================

// === PIN DEFINITIONS ===
#define LORA_NSS    18
#define LORA_RST    23
#define LORA_DIO0   26
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27

#define GPS_RX      12
#define GPS_TX      15
#define GPS_BAUD    9600

#define OLED_SDA    21
#define OLED_SCL    22
#define OLED_ADDR   0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// === LORA ===
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
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === STATE ===
int packetsReceived = 0;
String lastNode = "None";
float lastActivity = 0;
int lastHotZones = 0;
unsigned long lastDisplayUpdate = 0;
int displayPage = 0;

// === ESP-NOW Receive ===
void onDataReceived(const uint8_t * mac, const uint8_t * data, int len) {
  packetsReceived++;

  // Parse basic info from JSON (simple string search for demo)
  String payload = String((char*)data);
  if (payload.indexOf("node") != -1) {
    int start = payload.indexOf("node") + 7;
    int end = payload.indexOf('"', start);
    if (end > start) lastNode = payload.substring(start, end);
  }

  Serial.printf("[ESP-NOW] Packet #%d received\n", packetsReceived);

  // Forward via LoRa
  LoRa.beginPacket();
  LoRa.write(data, len);
  LoRa.endPacket();

  // Forward via UDP
  if (WiFi.status() == WL_CONNECTED) {
    udp.beginPacket(UDP_TARGET_IP, UDP_TARGET_PORT);
    udp.write(data, len);
    udp.endPacket();
  }

  // Update display with latest node info
  lastDisplayUpdate = millis();
  displayPage = 1; // Show node info
}

// === Display Update ===
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (displayPage == 0) {
    // Main status page
    display.setCursor(0, 0);
    display.println("LoraW10 Gateway");
    display.setCursor(0, 16);
    display.printf("Packets: %d", packetsReceived);
    display.setCursor(0, 28);
    if (gps.location.isValid()) {
      display.printf("GPS: %.4f,%.4f", gps.location.lat(), gps.location.lng());
    } else {
      display.println("GPS: No Fix");
    }
    display.setCursor(0, 40);
    display.printf("Last Node: %s", lastNode.c_str());

  } else if (displayPage == 1) {
    // Last node info
    display.setCursor(0, 0);
    display.println("Last Node:");
    display.setCursor(0, 16);
    display.printf("%s", lastNode.c_str());
    display.setCursor(0, 32);
    display.printf("Activity: %.2f", lastActivity);
    display.setCursor(0, 44);
    display.printf("HotZones: %d", lastHotZones);

    // Auto return to main page after 4 seconds
    if (millis() - lastDisplayUpdate > 4000) {
      displayPage = 0;
    }
  }

  display.display();
}

// === Gateway Heartbeat ===
void sendGatewayHeartbeat() {
  if (gps.location.isValid()) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"gw\":\"LoraW10\",\"lat\":%.6f,\"lon\":%.6f,\"sats\":%d}",
             gps.location.lat(), gps.location.lng(), gps.satellites.value());

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    Serial.printf("[LoRa] Heartbeat sent\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected: %s\n", WiFi.localIP().toString().c_str());
    udp.begin(4211);
  } else {
    Serial.println("\nRunning in LoRa-only mode");
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
    display.setCursor(0, 20);
    display.println("Booting...");
    display.display();
  }

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed");
    while (1);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  Serial.println("LoRa initialized");

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW Ready");

  Serial.println("=== LoraW10 Gateway Ready ===");
}

void loop() {
  // GPS parsing
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  // Update display periodically
  if (millis() - lastDisplayUpdate > 1500) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  // Send heartbeat every 30 seconds
  static unsigned long lastHB = 0;
  if (millis() - lastHB > 30000) {
    sendGatewayHeartbeat();
    lastHB = millis();
  }

  delay(10);
}
