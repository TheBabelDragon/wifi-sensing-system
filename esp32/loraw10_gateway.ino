#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// LoraW10 Gateway - Maximum Debug OLED Version
// Change the pins below if your board is different
// ============================================================

// === EDIT THESE PINS IF NEEDED ===
#define LORA_NSS     18
#define LORA_RST     23
#define LORA_DIO0    26
#define LORA_SCK      5
#define LORA_MISO    19
#define LORA_MOSI    27

#define GPS_RX       12
#define GPS_TX       15
#define GPS_BAUD   9600

// Try these OLED pins first (most common on W10 boards)
#define OLED_SDA     21
#define OLED_SCL     22
#define OLED_RESET   -1
#define OLED_ADDR    0x3C

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
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
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
  display.display();
}

void setup() {
  // Start Serial as early as possible
  Serial.begin(115200);
  delay(500);
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
  Serial.println("GPS serial started");

  // OLED - Maximum debug version
  Serial.printf("Trying OLED on SDA=%d, SCL=%d, Addr=0x%02X\n", OLED_SDA, OLED_SCL, OLED_ADDR);

  Wire.begin(OLED_SDA, OLED_SCL);
  delay(150);

  bool oledSuccess = false;

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    oledSuccess = true;
    Serial.println("SUCCESS: OLED initialized at 0x3C");
  } else {
    Serial.println("Failed at 0x3C, trying 0x3D...");
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      oledSuccess = true;
      Serial.println("SUCCESS: OLED initialized at 0x3D");
    } else {
      Serial.println("FAILED: No OLED detected on either address!");
    }
  }

  if (oledSuccess) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("LoraW10 Gateway");
    display.setCursor(0, 20);
    display.println("OLED Working!");
    display.display();
    delay(800);
  }

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init FAILED");
    while (1);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  Serial.println("LoRa initialized successfully");

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED");
    while (1);
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW Ready");

  Serial.println("=== LoraW10 Gateway Fully Started ===");
}

void loop() {
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 1000) {
    if (display.width() > 0) {   // Only update if display initialized
      updateDisplay();
    }
    lastDisplay = millis();
  }

  static unsigned long lastHB = 0;
  if (millis() - lastHB > 30000) {
    // sendGatewayHeartbeat(); // Uncomment if you want LoRa heartbeats
    lastHB = millis();
  }

  delay(10);
}
