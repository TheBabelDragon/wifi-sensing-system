#include <Wire.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("=== I2C Scanner ===");

  // Most common pins on W10-style boards
  Wire.begin(21, 22);

  Serial.println("Scanning I2C on SDA=21, SCL=22...");

  byte error, address;
  int devices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.printf("I2C device found at address 0x%02X\n", address);
      devices++;
    }
  }

  if (devices == 0) {
    Serial.println("No I2C devices found on SDA=21, SCL=22");
  } else {
    Serial.printf("Done. Found %d device(s).\n", devices);
  }
}

void loop() {}
