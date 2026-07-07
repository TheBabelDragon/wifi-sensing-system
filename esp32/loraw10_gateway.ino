#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// ============================================================
// LoraW10 Gateway
// ESP-NOW + LoRa + GPS Gateway for WiFi CSI Swarm
// Generic for Meshnology W10 / TTGO T-Beam style boards
// ============================================================

// === PIN DEFINITIONS (Meshnology W10 / similar boards) ===
#define LORA_NSS    18
#define LORA_RST    23
#define LORA_DIO0   26
#define LORA_SCK     5
#define LORA_MISO   19
#define LORA_MOSI   27

#define GPS_RX      12   // GPS TX -> ESP32 RX
#define GPS_TX      15   // GPS RX -> ESP32 TX
#define GPS_BAUD    9600

// === LORA SETTINGS ===
#define LORA_FREQUENCY  915E6   // Change to 868E6 for EU
#define LORA_SYNC_WORD  0x12

// === CONFIG ===
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* UDP_TARGET_IP   = "192.168.1.100";
const uint16_t UDP_TARGET_PORT = 4210;

// === OBJECTS ===
WiFiUDP udp;
TinyGPSPlus gps;
HardwareSerial GPS_Serial(1);   // Use UART1 for GPS

// === ESP-NOW Receive ===
void onDataReceived(const uint8_t * mac, const uint8_t * data, int len) {
  Serial.printf("[ESP-NOW] Packet from %02X:%02X:%02X:%02X:%02X:%02X (%d bytes)\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);

  // Forward via LoRa
  LoRa.beginPacket();
  LoRa.write(data, len);
  LoRa.endPacket();

  // Also forward via UDP if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    udp.beginPacket(UDP_TARGET_IP, UDP_TARGET_PORT);
    udp.write(data, len);
    udp.endPacket();
  }

  Serial.write(data, len);
  Serial.println();
}

// === Send Gateway Heartbeat + GPS over LoRa ===
void sendGatewayHeartbeat() {
  if (gps.location.isValid()) {
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"gateway\":\"LoraW10\",\"lat\":%.6f,\"lon\":%.6f,\"sats\":%d,\"heartbeat\":true}",
             gps.location.lat(), gps.location.lng(), gps.satellites.value());

    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    Serial.printf("[LoRa] Gateway heartbeat sent: %s\n", payload);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // WiFi (optional for UDP forwarding)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());
    udp.begin(4211);
  } else {
    Serial.println("\nRunning without WiFi (LoRa only)");
  }

  // GPS
  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS initialized");

  // LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(125E3);
  Serial.println("LoRa initialized");

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (1);
  }
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("ESP-NOW ready");

  Serial.println("=== LoraW10 Gateway Ready ===");
}

void loop() {
  // Read GPS
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  // Send gateway heartbeat every 30 seconds
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 30000) {
    sendGatewayHeartbeat();
    lastHeartbeat = millis();
  }

  delay(10);
}
