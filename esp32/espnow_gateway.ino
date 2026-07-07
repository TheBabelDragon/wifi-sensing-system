#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WiFiUdp.h>

// ============================================================
// ESP-NOW Gateway
// Receives from ESP32 CSI nodes and forwards to central pipeline via UDP
// ============================================================

// === CONFIGURATION ===
const char* WIFI_SSID     = "YOUR_WIFI_SSID";      // Optional: for internet / central access
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* TARGET_IP   = "192.168.1.100";       // IP of your central pipeline machine
const uint16_t TARGET_PORT = 4210;                 // Must match what nodes expect

const uint16_t LOCAL_UDP_PORT = 4211;              // Port this gateway listens on (if needed)

WiFiUDP udp;

// === ESP-NOW Receive Callback ===
void onDataReceived(const uint8_t * mac, const uint8_t * incomingData, int len) {
  Serial.printf("[ESP-NOW] Received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X\n",
                len,
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Forward the raw JSON payload via UDP
  udp.beginPacket(TARGET_IP, TARGET_PORT);
  udp.write(incomingData, len);
  udp.endPacket();

  // Also print to Serial for debugging
  Serial.write(incomingData, len);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Start WiFi (needed for UDP forwarding)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while (true) delay(1000);
  }

  esp_now_register_recv_cb(onDataReceived);

  // Start UDP
  udp.begin(LOCAL_UDP_PORT);

  Serial.println("=== ESP-NOW Gateway Ready ===");
  Serial.printf("Forwarding to %s:%d\n", TARGET_IP, TARGET_PORT);
}

void loop() {
  // Nothing to do here - everything happens in the callback
  delay(10);
}
