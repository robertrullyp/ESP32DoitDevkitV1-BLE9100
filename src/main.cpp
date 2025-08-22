#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEScan.h>

// Ganti dengan MAC Address sensor BLE kamu
static BLEAddress bleSensorAddress("C0:00:00:04:02:FA");
static BLEUUID serviceUUID((uint16_t)0xFF01);
static BLEUUID charUUID((uint16_t)0xFF02);

static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEClient* pClient;
static bool deviceConnected = false;
bool firstData = true;
bool isConnected = false;
unsigned long lastAttemptTime = 0;
const unsigned long reconnectInterval = 5000; // 5 detik

struct SensorData {
  float do_mg_L;
  float do_percent;
  float temperature;
  float battery;
  bool backlight;
  bool hold;
  uint8_t checksum;
  bool valid;
};

SensorData lastData;

void setFlag(bool nyala) {
  uint8_t cmd[3];
  cmd[0] = 0x01;
  cmd[1] = nyala ? 0x01 : 0x00; // 0x01 untuk HOLD, 0x00 untuk BACKLIGHT
  cmd[2] = cmd[0] ^ cmd[1]; // Checksum
  if (pRemoteCharacteristic != nullptr && pRemoteCharacteristic->canWrite()) {
    pRemoteCharacteristic->writeValue(cmd, 3, true);
    Serial.printf("✉️ Kirim Command %s\n", nyala ? "HOLD" : "BACKLIGHT");
  }
}
bool delayNonBlocking(unsigned long long ms, unsigned long long& lastTime) {
  if (millis() - lastTime >= ms) {
    lastTime = millis();
    return true;
  }
  return false;
}
unsigned long long lastSendTime = 0;
void sendFlagLoop() {
  if (delayNonBlocking(60000, lastSendTime)) {
    setFlag(false);
    setFlag(false);
  }
}

bool isDataChanged(const SensorData& a, const SensorData& b) {
  return a.do_mg_L      != b.do_mg_L      ||
         a.do_percent   != b.do_percent   ||
         a.temperature  != b.temperature  ||
         a.battery      != b.battery      ||
         a.backlight    != b.backlight    ||
         a.hold         != b.hold;
}

void deCode(uint8_t *pValue, uint8_t len) {
  for (int i = len - 1; i > 0; i--) {
    uint8_t tmp = pValue[i];
    uint8_t hibit1 = (tmp & 0x55) << 1;
    uint8_t lobit1 = (tmp & 0xAA) >> 1;
    tmp = pValue[i - 1];
    uint8_t hibit = (tmp & 0x55) << 1;
    uint8_t lobit = (tmp & 0xAA) >> 1;
    pValue[i] = 0xFF - (hibit1 | lobit);
    pValue[i - 1] = 0xFF - (hibit | lobit1);
  }
}

uint16_t reverseBytes(uint8_t high, uint8_t low) {
  return (high << 8) | low;
}

uint8_t checksum(const uint8_t *data, size_t len) {
  uint8_t result = 0;
  for (size_t i = 0; i < len; ++i) {
    result = result^data[i];
  }
  return result;
}
uint8_t checksum_sum(const uint8_t *data, size_t len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < len; ++i) {
    sum += data[i];
  }
  return sum & 0xFF;  // ambil 8-bit LSB
}
uint8_t checksum_payload_only(const uint8_t *data, size_t len) {
  uint8_t result = 0;
  for (size_t i = 1; i < len - 1; ++i) {
    result ^= data[i];
  }
  return result;
}
void debugChecksum(const uint8_t* data, size_t len) {
  Serial.print("Data: ");
  for (size_t i = 0; i < len; ++i) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  uint8_t got = data[len - 1];
  uint8_t calc = checksum(data, len - 1);
  Serial.printf("📊 XOR Checksum: got %02X, calc %02X\n", got, calc);
  uint8_t sum = checksum_sum(data, len - 1);
  Serial.printf("📊 SUM Checksum: got %02X, calc %02X\n", got, sum);
  uint8_t alt = checksum_payload_only(data, len);
  Serial.printf("📊 Payload XOR:  got %02X, calc %02X\n", got, alt);
}

SensorData decodeData(uint8_t* rawData, size_t len) {
  SensorData data = {};
  data.valid = false;
  if (len < 24) return data;
  uint8_t decoded[32];
  memcpy(decoded, rawData, len);
  deCode(decoded, len);
  // uint8_t calculatedChecksum = checksum(decoded, 23);
  // data.checksum = decoded[23];
  // if (calculatedChecksum != data.checksum) {
  //   debugChecksum(rawData, len);
  //   Serial.printf("❌ Checksum mismatch: expected %02X, got %02X\n", calculatedChecksum, data.checksum);
  //   debugChecksum(decoded, len);
  //   return data;
  // }
  data.do_mg_L = reverseBytes(decoded[3], decoded[4]) / 100.0;
  data.do_percent = reverseBytes(decoded[5], decoded[6]) / 10.0;
  data.temperature = reverseBytes(decoded[13], decoded[14]) / 10.0;
  data.battery = reverseBytes(decoded[15], decoded[16]) / 100.0;
  uint8_t flags = decoded[17];
  data.hold      = (flags >> 4) & 0x01;       // ambil bit 4
  data.backlight = (flags >> 3) & 0x01;       // ambil bit 3
  data.valid = true;

  return data;
}
// SensorData decodeData(uint8_t* rawData, size_t len) {
//   SensorData data = {};
//   data.valid = false;
//   if (len < 24) return data;
//   uint8_t decoded[32];
//   memcpy(decoded, rawData, len);
//   deCode(decoded, len);
//   data.ec         = reverseBytes(decoded[5],  decoded[6]);
//   data.tds        = reverseBytes(decoded[7],  decoded[8]);
//   data.salt_tds   = reverseBytes(decoded[9],  decoded[10]);
//   data.salt_sg    = reverseBytes(decoded[11], decoded[12]);
//   data.ph         = reverseBytes(decoded[3],  decoded[4]) / 100.0;
//   data.temperature= reverseBytes(decoded[13], decoded[14]) / 10.0;
//   data.battery    = reverseBytes(decoded[15], decoded[16]);
//   data.orp        = reverseBytes(decoded[20], decoded[21]);
//   uint8_t flags = decoded[17];
//   data.backlight = (flags & 0b00000010) >> 1;
//   data.hold      = (flags & 0b00000001);
//   data.checksum = decoded[len - 1];
//   data.valid = true;
//   return data;
// }

void printDebugInfo(const uint8_t* data, size_t length) {
  if (length < 24) {
    Serial.println("⚠️  Data terlalu pendek untuk debug.");
    return;
  }
  Serial.print("📥 Raw Data ("); Serial.print(length); Serial.print(" bytes): ");
  for (size_t i = 0; i < length; ++i) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  uint8_t buffer[32];
  memcpy(buffer, data, length);
  deCode(buffer, length);
  Serial.print("🧠 Decoded Data: ");
  for (size_t i = 0; i < length; ++i) {
    Serial.printf("%02X ", buffer[i]);
  }
  Serial.println();
  Serial.printf("🔍 pH        : %02X %02X ➤ %.2f\n", buffer[3],  buffer[4],  reverseBytes(buffer[3], buffer[4]) / 100.0);
  Serial.printf("🌡️  Temp      : %02X %02X ➤ %.1f °C\n", buffer[13], buffer[14], reverseBytes(buffer[13], buffer[14]) / 10.0);
  Serial.printf("🔋 Battery   : %02X %02X ➤ %d\n", buffer[15], buffer[16], reverseBytes(buffer[15], buffer[16]));
  Serial.printf("💡 Flags[17] : %02X ➤ Backlight=%d, Hold=%d\n", buffer[17], (buffer[17] >> 1) & 1, buffer[17] & 1);
}

SensorData pollSensorDataIfDue(BLERemoteCharacteristic* pChar, unsigned long interval = 1000) {
  static unsigned long lastMillis = 0;
  static SensorData lastValidData = {};
  SensorData result = {};
  result.valid = false;
  if (!pChar || !pChar->canRead()) return result;
  unsigned long now = millis();
  if (now - lastMillis < interval) return result;
  lastMillis = now;
  std::string value;
  try {
    value = pChar->readValue();
  } catch (...) {
    Serial.println("❌ Gagal membaca value dari BLE characteristic.");
    deviceConnected = false;
    return result;
  }
  if (value.length() < 24) {
    Serial.printf("⚠️ Data terlalu pendek: %d byte\n", value.length());
    return result;
  }
  result = decodeData((uint8_t*)value.data(), value.length());
  if (result.valid) {
    // Optional: tampilkan log
    // Serial.printf("✅ Polled ➤ DO : %.2f mg/L | DO %: %.2f %% | Temp: %.2f°C | Bat: %.2fV | Backlight: %s | Hold: %s\n",
    //   result.do_mg_L, result.do_percent, result.temperature, result.battery, result.backlight ? "ON" : "OFF", result.hold ? "ON" : "OFF");
    lastValidData = result;
  } else {
    Serial.println("⚠️ Data hasil polling tidak valid (checksum mismatch?).");
  }
  return result;
}

void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  printDebugInfo(pData, length);
  SensorData result = decodeData(pData, length);
  if (!result.valid) {
    Serial.println("❌ Decode gagal, data tidak valid.");
    return;
  }
  if (firstData || isDataChanged(result, lastData)) {
    firstData = false;
    lastData = result;
    Serial.println("✅ Sensor Data Baru:");
    Serial.printf("   DO         : %.2f\n", result.do_mg_L);
    Serial.printf("   DO         : %.2f\n", result.do_percent);
    Serial.printf("   Temp       : %.2f\n", result.temperature);
    Serial.printf("   Battery    : %.2f\n", result.battery);
    Serial.printf("   Backlight  : %s\n", result.backlight ? "ON" : "OFF");
    Serial.printf("   Hold       : %s\n", result.hold ? "ON" : "OFF");
  }
}


void connectToBle() {
  deviceConnected = false;
  Serial.println("🔍 Scanning BLE...");
  BLEScan* pScan = BLEDevice::getScan();
  pScan->setActiveScan(true);
  BLEScanResults foundDevices = pScan->start(5);
  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    Serial.print("📡 Ditemukan: ");
    Serial.println(device.getAddress().toString().c_str());
    if (device.getAddress().equals(bleSensorAddress)) {
      Serial.println("✅ Sensor ditemukan!");
      pClient = BLEDevice::createClient();
      if (!pClient->connect(&device)) {
        Serial.println("❌ Gagal connect ke device.");
        return;
      }
      BLERemoteService* pService = pClient->getService(serviceUUID);
      if (!pService) {
        Serial.println("❌ Service tidak ditemukan.");
        return;
      }
      pRemoteCharacteristic = pService->getCharacteristic(charUUID);
      if (!pRemoteCharacteristic) {
        Serial.println("❌ Characteristic tidak ditemukan.");
        return;
      }
      if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
        Serial.println("📶 Notifikasi didaftarkan");
        BLERemoteDescriptor* pDesc = pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pDesc) {
          uint8_t notifOn[] = {0x01, 0x00};
          pDesc->writeValue(notifOn, 2, true);
          Serial.println("📶 Notifikasi diaktifkan lewat descriptor.");
        }
        deviceConnected = true;
        return;
      }
    }
  }
  Serial.println("❌ Sensor tidak ditemukan");
}

bool delayNonBlocking(unsigned long interval) {
  static unsigned long lastMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastMillis >= interval) {
    lastMillis = currentMillis;
    return true;
  }
  return false;
}

void printSensor(const SensorData& result) {
  Serial.printf("   DO         : %.2f mg/L\n", result.do_mg_L);
  Serial.printf("   DO         : %.2f %%\n", result.do_percent);
  Serial.printf("   Temp       : %.2f °C\n", result.temperature);
  Serial.printf("   Battery    : %.2f V\n", result.battery);
  Serial.printf("   Backlight  : %s\n", result.backlight ? "ON" : "OFF");
  Serial.printf("   Hold       : %s\n", result.hold ? "ON" : "OFF");
}

void setup() {
  Serial.begin(115200);
  BLEDevice::init("");
  connectToBle();
}

void loop() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // hapus spasi & newline
    if (input == "1") {
      setFlag(true); // Toggle HOLD
    } else if (input == "2") {
      setFlag(false); // Toggle BACKLIGHT)
    }
  }
  if (!deviceConnected) {
    unsigned long now = millis();
    if (now - lastAttemptTime > reconnectInterval) {
      lastAttemptTime = now;
      Serial.println("🔄 Mencoba reconnect BLE...");
      connectToBle(); // panggil ulang
    }
    return; // skip polling data
  }
  // Fallback polling jika notify tidak jalan
  SensorData data = pollSensorDataIfDue(pRemoteCharacteristic, 1000);
  if (data.valid && (firstData || isDataChanged(data, lastData))) {
    firstData = false;
    lastData = data;
    Serial.println("🌀 [Polling] Data baru:");
    printSensor(data);
  }
  sendFlagLoop();
}
