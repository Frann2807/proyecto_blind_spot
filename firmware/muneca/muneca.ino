// ============================================================
// BLIND SPOT - FIRMWARE MUÑECA (ESP32-C3 Super Mini)
// Estilo BalancinCyS: BLE (NimBLE NUS) + interprete de comandos.
//
// - Pulsador en GPIO3 (INPUT_PULLDOWN, boton entre 3V3 y GPIO3):
//   al presionarlo envia por ESP-NOW "btn" al baston, que activa
//   su buzzer (localizador del baston).
// - Transistor en GPIO10 -> motor vibrador: vibra cuando el
//   baston avisa por ESP-NOW ("obst" = obstaculo, "oscuro",
//   "ping").
// - BLE para debug inalambrico: los comandos simulan la
//   pulsacion del boton y los mensajes entrantes del baston,
//   para poder probar TODO con una sola ESP32-C3.
// ============================================================

#include "espnow_cb.h"

#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ==================================================
// == PINES (ESP32-C3 Super Mini) ==
// ==================================================
#define PIN_BOTON 3    // pulsador a 3V3, pulldown interno
#define PIN_VIB   10   // transistor -> motor vibrador

// Broadcast: funciona sin conocer la MAC del baston (alcanza con
// que las dos ESP esten en el mismo canal, 1). Para dirigirse
// solo al baston, reemplazar por su MAC (comando 'mac' del baston).
uint8_t macPeer[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Banderas fijadas en el callback de recepcion (tarea WiFi),
// procesadas en loop para no hacer I/O dentro del callback.
volatile bool flagObst   = false;
volatile bool flagPing   = false;
volatile bool flagOscuro = false;
volatile bool flagClaro  = false;
volatile int  lastValor  = 0;
volatile bool lastSendOK = false;

// ==================================================
// == CONFIGURACION BLE (NORDIC UART SERVICE) ==
// ==================================================
#define NUS_SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static NimBLEServer* pServer;
static NimBLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

Preferences prefs;
const char* NVS_NAMESPACE = "muneca";

// ==================================================
// == PARAMETROS CALIBRABLES (por BLE, con 'guardar') ==
// ==================================================
volatile int pwmVib = 255;  // intensidad del vibrador (0-255)
volatile int pulso  = 200;  // ms encendido por pulso
volatile int pausa  = 150;  // ms entre pulsos
volatile int npul   = 2;    // pulsos al recibir "obst"

// ==================================================
// == ESTADO DEL VIBRADOR (no bloqueante) ==
// ==================================================
enum { VIB_IDLE, VIB_ON, VIB_OFF_WAIT };
int vibEstado = VIB_IDLE;
int vibRestantes = 0;
unsigned long vibT0 = 0;

// ==================================================
// == PROTOTIPOS ==
// ==================================================
void btPrint(String s);
void btPrintln(String s);
void btNotificar(String s);
void btFlushCola();
void comandProcess(String cmd);
void mostrarAyuda();
void mostrarValores();
bool enviar(const char* tipo, int valor);
void vibrar(int veces);
void vibLoop();
void OnDataSent(const uint8_t* mac, esp_now_send_status_t status);

// ==================================================
// == CALLBACKS BLE ==
// ==================================================
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
      if (cmd.length() > 0) comandProcess(cmd);
    }
    if (rxBuffer.length() > 256) rxBuffer = "";
  }
};

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.println("BLE conectado");
  }
  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("BLE desconectado");
    NimBLEDevice::startAdvertising();
  }
};

// ==================================================
// == SETUP ==
// ==================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  prefs.begin(NVS_NAMESPACE, true);
  if (prefs.isKey("pwm")) {
    pwmVib = prefs.getInt("pwm", pwmVib);
    pulso  = prefs.getInt("pulso", pulso);
    pausa  = prefs.getInt("pausa", pausa);
    npul   = prefs.getInt("npul", npul);
    Serial.println("Parametros restaurados desde flash.");
  }
  prefs.end();

  // BLE
  NimBLEDevice::init("MunecaBS");
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
  pAdvertising->setName("MunecaBS");
  pAdvertising->addServiceUUID(pNusService->getUUID());
  pAdvertising->start();
  Serial.println("BLE listo, esperando conexion...");

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() == ESP_OK) {
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(ESPNOW_RECV_CB);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, macPeer, 6);
    peer.channel = 1;
    peer.encrypt = false;
    esp_err_t r = esp_now_add_peer(&peer);
    if (r == ESP_OK) Serial.println("ESP-NOW listo (broadcast, canal 1).");
    else if (r == ESP_ERR_ESPNOW_EXIST) Serial.println("Peer ESP-NOW ya existia.");
    else Serial.println("No se pudo agregar el peer ESP-NOW.");
  } else {
    Serial.println("ESP-NOW NO inicializo. Solo funcionara BLE/Serial.");
  }

  // Salidas / entradas
  pinMode(PIN_VIB, OUTPUT);
  digitalWrite(PIN_VIB, LOW);
  analogWriteResolution(PIN_VIB, 8);
  pinMode(PIN_BOTON, INPUT_PULLDOWN);

  Serial.printf("MAC (para el peer del baston): %s\n", WiFi.macAddress().c_str());
  Serial.println("Setup completo. Escribir 'help' por BLE o Serial.");
}

// ==================================================
// == LOOP ==
// ==================================================
void loop() {
  // A. BLE
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = false;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = true;
  }

  // B. Banderas ESP-NOW (callback -> loop)
  if (flagObst) {
    flagObst = false;
    btPrintln("RX baston: OBSTACULO (IR=" + String(lastValor) + ") -> vibrando");
    vibrar(npul);
  }
  if (flagPing) {
    flagPing = false;
    btPrintln("RX baston: PING -> pulso de respuesta");
    vibrar(1);
  }
  if (flagOscuro) {
    flagOscuro = false;
    btPrintln("RX baston: OSCURO -> pulso corto");
    vibrar(1);
  }
  if (flagClaro) {
    flagClaro = false;
    btPrintln("RX baston: claro");
  }

  // C. Pulsador (debounce 50 ms; presionado = HIGH por el pulldown)
  static bool btnEstable = LOW;
  static unsigned long btnT = 0;
  bool lec = digitalRead(PIN_BOTON);
  if (lec != btnEstable) {
    if (millis() - btnT > 50) {
      btnEstable = lec;
      btnT = millis();
      if (lec == HIGH) {
        btPrintln("BOTON presionado -> envio 'btn' al baston");
        enviar("btn", 0);
        vibrar(1);  // confirmacion local
      }
    }
  }

  // D. Vibrador
  vibLoop();

  // E. Cola BLE
  btFlushCola();

  delay(20);
}

// ==================================================
// == ESP-NOW ==
// ==================================================
void OnDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  (void)mac;
  lastSendOK = (status == ESP_NOW_SEND_SUCCESS);
}

void onRecvMensaje(const uint8_t* mac, const Mensaje* m) {
  (void)mac;
  lastValor = m->valor;
  if (strncmp(m->tipo, "obst", 4) == 0) flagObst = true;
  else if (strncmp(m->tipo, "ping", 4) == 0) flagPing = true;
  else if (strncmp(m->tipo, "oscuro", 6) == 0) flagOscuro = true;
  else if (strncmp(m->tipo, "claro", 5) == 0) flagClaro = true;
}

bool enviar(const char* tipo, int valor) {
  Mensaje m;
  strncpy(m.tipo, tipo, sizeof(m.tipo));
  m.valor = valor;
  esp_err_t r = esp_now_send(macPeer, (uint8_t*)&m, sizeof(m));
  if (r != ESP_OK) btPrintln("Error ESP-NOW: " + String(esp_err_to_name(r)));
  return r == ESP_OK;
}

// ==================================================
// == VIBRADOR (maquina de estados no bloqueante) ==
// ==================================================
void vibrar(int veces) {
  if (veces <= 0) veces = 1;
  vibRestantes = veces;
  vibEstado = VIB_ON;
  vibT0 = millis();
  if (pwmVib >= 255) digitalWrite(PIN_VIB, HIGH);
  else analogWrite(PIN_VIB, pwmVib);
}

void vibLoop() {
  if (vibEstado == VIB_IDLE) return;
  unsigned long now = millis();
  if (vibEstado == VIB_ON && now - vibT0 >= (unsigned long)pulso) {
    digitalWrite(PIN_VIB, LOW);
    vibRestantes--;
    if (vibRestantes <= 0) {
      vibEstado = VIB_IDLE;
      return;
    }
    vibEstado = VIB_OFF_WAIT;
    vibT0 = now;
  } else if (vibEstado == VIB_OFF_WAIT && now - vibT0 >= (unsigned long)pausa) {
    vibEstado = VIB_ON;
    vibT0 = now;
    if (pwmVib >= 255) digitalWrite(PIN_VIB, HIGH);
    else analogWrite(PIN_VIB, pwmVib);
  }
}

// ==================================================
// == SALIDA BLE (COLA) ==
// ==================================================
String txCola = "";
portMUX_TYPE txMux = portMUX_INITIALIZER_UNLOCKED;

void btNotificar(String s) {
  if (!deviceConnected) return;
  portENTER_CRITICAL(&txMux);
  txCola += s;
  if (txCola.length() > 8192) txCola = "";
  portEXIT_CRITICAL(&txMux);
}

void btPrint(String s) {
  Serial.print(s);
  btNotificar(s);
}

void btPrintln(String s) {
  btPrint(s + "\n");
}

void btFlushCola() {
  if (txCola.length() == 0) return;
  if (!deviceConnected || pTxCharacteristic == nullptr) {
    portENTER_CRITICAL(&txMux);
    txCola = "";
    portEXIT_CRITICAL(&txMux);
    return;
  }
  const int CHUNK = 20;
  const int MAX_ENVIOS = 3;
  for (int i = 0; i < MAX_ENVIOS; i++) {
    String chunk;
    portENTER_CRITICAL(&txMux);
    if (txCola.length() == 0) {
      portEXIT_CRITICAL(&txMux);
      break;
    }
    chunk = txCola.substring(0, CHUNK);
    txCola = txCola.substring(CHUNK);
    portEXIT_CRITICAL(&txMux);
    pTxCharacteristic->setValue(chunk.c_str());
    pTxCharacteristic->notify();
  }
}

// ==================================================
// == INTERPRETE DE COMANDOS ==
// ==================================================
void comandProcess(String cmd) {
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() < 1) return;

  int sp = cmd.indexOf(' ');
  String nombre = (sp < 0) ? cmd : cmd.substring(0, sp);
  String valorStr = (sp < 0) ? "" : cmd.substring(sp + 1);
  valorStr.trim();
  float valor = (valorStr.length() > 0) ? valorStr.toFloat() : 0;

  if (nombre == "help") { mostrarAyuda(); return; }
  if (nombre == "mostrar") { mostrarValores(); return; }
  if (nombre == "mac") {
    btPrintln("MAC: " + WiFi.macAddress());
    return;
  }
  if (nombre == "btn") {
    btPrintln("SIMULANDO boton -> envio 'btn' al baston");
    enviar("btn", 0);
    vibrar(1);
    return;
  }
  if (nombre == "ping") {
    btPrintln("Enviando 'ping' al baston");
    enviar("ping", 0);
    return;
  }
  if (nombre == "vibra") {
    int veces = (valorStr.length() > 0) ? (int)valor : 1;
    btPrintln("Vibrando " + String(veces) + "x");
    vibrar(veces);
    return;
  }
  if (nombre == "simbaston") {
    // Simula un mensaje entrante del baston
    if (valorStr == "obst") { btPrintln("SIM baston: obstaculo"); flagObst = true; }
    else if (valorStr == "ping") { btPrintln("SIM baston: ping"); flagPing = true; }
    else if (valorStr == "oscuro") { btPrintln("SIM baston: oscuro"); flagOscuro = true; }
    else if (valorStr == "claro") { btPrintln("SIM baston: claro"); flagClaro = true; }
    else btPrintln("Uso: simbaston <obst|ping|oscuro|claro>");
    return;
  }
  if (nombre == "guardar") {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("pwm", pwmVib);
    prefs.putInt("pulso", pulso);
    prefs.putInt("pausa", pausa);
    prefs.putInt("npul", npul);
    prefs.end();
    btPrintln("Guardado en flash.");
    return;
  }

  if (valorStr.length() == 0) {
    btPrintln("Falta el valor. Escribir 'help'.");
    return;
  }

  if (nombre == "pwm") pwmVib = constrain((int)valor, 0, 255);
  else if (nombre == "pulso") pulso = constrain((int)valor, 20, 2000);
  else if (nombre == "pausa") pausa = constrain((int)valor, 20, 2000);
  else if (nombre == "npul") npul = constrain((int)valor, 1, 10);
  else {
    btPrintln("Comando no reconocido. Escribir 'help'.");
    return;
  }
  btPrintln(nombre + " = " + valorStr);
}

void mostrarAyuda() {
  btPrintln("ACCIONES: btn ping vibra simbaston mac mostrar guardar help");
  btPrintln("simbaston: <obst|ping|oscuro|claro> (simula mensaje del baston)");
  btPrintln("PARAMETROS: pwm pulso pausa npul");
}

void mostrarValores() {
  btPrintln("===== VALORES =====");
  btPrintln("pwm=" + String(pwmVib) + " pulso=" + String(pulso) +
            " pausa=" + String(pausa) + " npul=" + String(npul));
  btPrintln("===================");
}
