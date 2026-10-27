// ============================================================
// TEST 08 - BLE (NimBLE NUS) (ESP32-C3 Super Mini)
// - Crea el servicio NUS ("Nordic UART Service") igual que los
//   firmwares finales y hace ECO de lo recibido por BLE.
// - Comandos: help | hola | mac | ping
// - Probar con 'nRF Connect' o 'Serial Bluetooth Terminal'.
//   Nombre del dispositivo: TestBLE_BS
// ============================================================

#include <NimBLEDevice.h>
#include <WiFi.h>

#define NUS_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLEServer* pServer;
static NimBLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

void btPrintln(String s) {
  Serial.println(s);
  if (deviceConnected && pTxCharacteristic != nullptr) {
    pTxCharacteristic->setValue((s + "\n").c_str());
    pTxCharacteristic->notify();
  }
}

class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  String rxBuffer;
  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
    std::string value = pCharacteristic->getValue();
    if (value.length() == 0) return;
    rxBuffer += String(value.c_str());
    int idx;
    while ((idx = rxBuffer.indexOf('\n')) >= 0) {
      String cmd = rxBuffer.substring(0, idx);
      rxBuffer = rxBuffer.substring(idx + 1);
      cmd.trim();
      cmd.toLowerCase();
      if (cmd.length() == 0) continue;
      btPrintln("ECO: " + cmd);
      if (cmd == "help") {
        btPrintln("Comandos: help | hola | mac | ping");
      } else if (cmd == "hola") {
        btPrintln("Hola! BLE funcionando.");
      } else if (cmd == "mac") {
        btPrintln("MAC: " + WiFi.macAddress());
      } else if (cmd == "ping") {
        btPrintln("PONG");
      } else {
        btPrintln("Comando no reconocido ('help').");
      }
    }
    if (rxBuffer.length() > 256) rxBuffer = "";
  }
};

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.println("BLE conectado");
    btPrintln("BLE conectado.");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("BLE desconectado");
    NimBLEDevice::startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  WiFi.mode(WIFI_STA);  // para que 'mac' devuelva una MAC valida
  Serial.println("TEST08: BLE (NimBLE NUS)");

  NimBLEDevice::init("Jaz_BLE");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  NimBLEService* pNusService = pServer->createService(NUS_SERVICE_UUID);
  NimBLECharacteristic* pRxCharacteristic =
      pNusService->createCharacteristic(NUS_CHARACTERISTIC_UUID_RX, NIMBLE_PROPERTY::WRITE);
  pRxCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pTxCharacteristic =
      pNusService->createCharacteristic(NUS_CHARACTERISTIC_UUID_TX, NIMBLE_PROPERTY::NOTIFY);
  pNusService->start();
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("TestBLE_BS");
  pAdvertising->addServiceUUID(pNusService->getUUID());
  pAdvertising->start();
  Serial.println("BLE listo. Conectar con nRF Connect / Serial Bluetooth Terminal.");
}

void loop() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = false;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = true;
  }
  delay(20);
}
