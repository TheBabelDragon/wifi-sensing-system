static void emitCsiJson(char* buf, size_t buflen, size_t* n_out) {
  StaticJsonDocument<1500> doc;
  doc["node"] = NODE_ID;
  doc["body_id"] = NODE_ID;
  doc["body_type"] = "wifi_csi";
  uint32_t ts = millis();
  doc["timestamp"] = ts;
  doc["rssi"] = wifiConnected ? (int)WiFi.RSSI() : -100;
  doc["type"] = "wifi_csi";
  doc["source_class"] = "physical";
  doc["synthetic"] = false;
  doc["band"] = currentBandLabel();
  doc["channel"] = WiFi.channel();
  doc["activity"] = activityLevel;
  doc["hot_zones"] = hotZoneCount;
  doc["obstruction"] = significantObstruction;
  doc["movement_intensity"] = movementIntensity;
  doc["confidence"] = nodeConfidence;
  doc["excitement"] = excitementLevel;
  doc["rf_busy"] = rfBusyness;
  doc["interval_ms"] = sendIntervalMs;
  doc["boost"] = boostLevel;
  doc["via"] = "udp";
  doc["csi_irq"] = (uint32_t)csiIrqCount;
  char ah[9];
  makeAuthHex(ah, NODE_ID, ts);
  doc["auth"] = ah;
  JsonArray bandMov = doc.createNestedArray("band_movement");
  for (int b = 0; b < 4; b++) bandMov.add(bandMovement[b]);
  JsonArray csiArr = doc.createNestedArray("csi");
  for (int i = 0; i < 32; i++) csiArr.add(latestRealCSI[i]);
  *n_out = serializeJson(doc, buf, buflen);
}

static void udpSendBuf(const char* buf, size_t n, IPAddress dest) {
  if (n == 0) return;
  if (udpCsi.beginPacket(dest, CSI_PORT)) {
    udpCsi.write((const uint8_t*)buf, n);
    udpCsi.endPacket();
  }
}

void sendUdpCsi() {
  char buf[1500];
  size_t n = 0;
  emitCsiJson(buf, sizeof(buf), &n);
  Serial.print("CSIJSON ");
  Serial.write((const uint8_t*)buf, n);
  Serial.println();
  if (wifiConnected) {
    IPAddress subnet = WiFi.localIP();
    subnet[3] = 255;
    udpSendBuf(buf, n, subnet);
    udpSendBuf(buf, n, IPAddress(255,255,255,255));
    if (hostUnicast[0] != 0) udpSendBuf(buf, n, hostUnicast);
    packetCount++;
  }
}

void sendCSIPacket() {
  consumeNewCsi();
  sendEspNowCsi();
  sendUdpCsi();
}
