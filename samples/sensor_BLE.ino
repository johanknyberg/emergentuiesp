#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "HX711.h"

// —————— HX711 SETUP ——————
const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN  = 4;
HX711 scale;

// —————— BLE SETUP ——————
static BLEUUID SERVICE_UUID        ("12345678-1234-1234-1234-1234567890ab");
static BLEUUID CHARACTERISTIC_UUID ("abcd1234-ab12-cd34-ef56-abcdef123456");
BLECharacteristic* pCharacteristic;
bool deviceConnected = false;

// —————— TIMING ——————
// How often to send updates (in milliseconds)
const unsigned long UPDATE_INTERVAL_MS = 200;

// —————— WRITE CALLBACK ——————
class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    String rx = pChar->getValue();
    if (rx.length() > 0) {
      Serial.print("Received from client: ");
      Serial.println(rx);
    }
  }
};

// —————— CONNECT / DISCONNECT CALLBACKS ——————
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    deviceConnected = true;
    Serial.println("Client connected");
  }
  void onDisconnect(BLEServer* server) override {
    deviceConnected = false;
    Serial.println("Client disconnected");
    server->getAdvertising()->start();
    Serial.println("Advertising restarted");
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE + HX711 scale");

  // Initialize and tare the HX711 scale
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(2280.f); // <-- calibrate this to your loadcell
  scale.tare();            // zero the scale
  Serial.println("Scale initialized and tared");

  // Initialize BLE
  BLEDevice::init("ESP32_Scale");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create the BLE Characteristic with read/write/notify
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ      |
    BLECharacteristic::PROPERTY_WRITE     |
    BLECharacteristic::PROPERTY_WRITE_NR  |
    BLECharacteristic::PROPERTY_NOTIFY    |
    BLECharacteristic::PROPERTY_INDICATE
  );

  // Attach callback to log incoming writes
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start service & advertising
  pService->start();
  pServer->getAdvertising()
         ->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()
         ->start();
  Serial.println("BLE service started, advertising as ESP32_Scale");
}

void loop() {
  static unsigned long lastMillis = 0;

  // If connected and the interval has elapsed, send a new reading
  if (deviceConnected && (millis() - lastMillis) >= UPDATE_INTERVAL_MS) {
    lastMillis = millis();

    // Read the weight (average of 10 readings)
    float weight = scale.get_units(10);

    // Format to two decimals
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", weight);

    // Send via BLE notify
    pCharacteristic->setValue(buf);
    pCharacteristic->notify();

    // Also log locally
    Serial.print("[");
    Serial.print(lastMillis);
    Serial.print(" ms] Sent weight: ");
    Serial.println(buf);
  }
}
