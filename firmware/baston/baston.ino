// ============================================================
// BLIND SPOT - FIRMWARE BASTON (ESP32-C3 Super Mini)
// Estilo BalancinCyS: BLE (NimBLE NUS) + interprete de comandos.
//
// - Sensor IR analogico en GPIO0 (ADC1_CH0): detecta obstaculos
//   y avisa por ESP-NOW a la muñeca ("obst"/"libre"); repite
//   "obst" cada cdobs ms mientras el obstaculo siga presente.
// - Fotoresistencia en GPIO1 (ADC1_CH1): segun los umbrales
//   enciende LED1 (GPIO5) y LED2 (GPIO6) y avisa por ESP-NOW
//   "oscuro"/"claro" a la muñeca.
// - Buzzer ACTIVO en GPIO4: suena cuando la muñeca envia "btn"
//   (localizador del baston) o "ping".
// - BLE para debug inalambrico: los comandos simulan los
//   mensajes de la muñeca y accionan cada salida, para probar
//   todo con una sola ESP32-C3.
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
#define PIN_IR    0   // sensor IR analogico (ADC1_CH0)
#define PIN_FOTO  1   // fotoresistencia (ADC1_CH1)
#define PIN_BUZZ  4   // buzzer activo
#define PIN_LED1  5   // LED 1 (oscuridad nivel 1)
#define PIN_LED2  6   // LED 2 (oscuridad nivel 2)

// Broadcast: funciona sin conocer la MAC de la muñeca (canal 1).
// Para dirigirse solo a la muñeca, reemplazar por su MAC (comando
// 'mac' de la muñeca).
uint8_t macPeer[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Banderas fijadas en el callback de recepcion (tarea WiFi),
// procesadas en loop para no hacer I/O dentro del callback.
volatile bool flagBtn  = false;
volatile bool flagPing = false;
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
const char* NVS_NAMESPACE = "baston";

// ==================================================
// == PARAMETROS CALIBRABLES (por BLE, con 'guardar') ==
// ==================================================
volatile int umbral = 2500;  // IR: lectura que dispara obstaculo
volatile int dirir   = 1;    // 1: obstaculo si IR > umbral | -1: si IR < umbral
volatile int luz1    = 2000; // foto: umbral del LED1
volatile int luz2    = 3000; // foto: umbral del LED2
volatile int fdir    = 1;    // 1: oscuro si foto > umbral | -1: si foto < umbral
volatile int cdobs   = 500;  // ms minimos entre envios de "obst"
volatile int nprom   = 10;   // muestras promediadas por lectura

// Control de LEDs
volatile int ledOverride = 1;  // 1 = automatico (umbrales), 0 = manual
volatile int manLed1 = 0;
volatile int manLed2 = 0;

volatile int stream = 0;  // 1 = stream de sensores por BLE/Serial

// Estado
bool obstActual = false;
bool oscActual = false;
unsigned long lastEnvio = 0;

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
int leerIR();
int leerFoto();
void beep(int onMs);
void sonarPatron(int veces, int onMs, int offMs);
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
  analogReadResolution(12);

  prefs.begin(NVS_NAMESPACE, true);
  if (prefs.isKey("umbral")) {
    umbral = prefs.getInt("umbral", umbral);
    dirir  = prefs.getInt("dirir", dirir);
    luz1   = prefs.getInt("luz1", luz1);
    luz2   = prefs.getInt("luz2", luz2);
    fdir   = prefs.getInt("fdir", fdir);
    cdobs  = prefs.getInt("cdobs", cdobs);
    nprom  = prefs.getInt("nprom", nprom);
    Serial.println("Parametros restaurados desde flash.");
  }
  prefs.end();

  // Salidas
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_BUZZ, LOW);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);

  // BLE
  NimBLEDevice::init("BastonBS");
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
  pAdvertising->setName("BastonBS");
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

  // Semilla de estados: evita avisos falsos al arrancar
  long sIr = 0, sFo = 0;
  for (int i = 0; i < 10; i++) {
    sIr += analogRead(PIN_IR);
    sFo += analogRead(PIN_FOTO);
    delay(1);
  }
  int ir0 = sIr / 10, fo0 = sFo / 10;
  obstActual = (dirir == 1) ? (ir0 > umbral) : (ir0 < umbral);
  oscActual  = (fdir == 1) ? (fo0 > luz2) : (fo0 < luz2);

  Serial.printf("MAC (para el peer de la muñeca): %s\n", WiFi.macAddress().c_str());
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
  if (flagBtn) {
    flagBtn = false;
    btPrintln("RX muñeca: BOTON -> buzzer de localizacion");
    sonarPatron(3, 150, 100);
  }
  if (flagPing) {
    flagPing = false;
    btPrintln("RX muñeca: PING");
    beep(300);
  }

  // C. Sensores
  int ir = leerIR();
  int foto = leerFoto();

  // D. Obstaculo (IR) -> aviso a la muñeca
  bool obst = (dirir == 1) ? (ir > umbral) : (ir < umbral);
  if (obst != obstActual) {
    obstActual = obst;
    btPrintln("IR: " + String(ir) + (obst ? " -> OBSTACULO" : " -> libre"));
    enviar(obst ? "obst" : "libre", ir);
    lastEnvio = millis();
  } else if (obst && millis() - lastEnvio >= (unsigned long)cdobs) {
    // Repite mientras el obstaculo siga ahi (por si se pierde un paquete)
    enviar("obst", ir);
    lastEnvio = millis();
  }

  // E. Fotoresistencia -> LEDs + aviso
  bool osc1 = (fdir == 1) ? (foto > luz1) : (foto < luz1);
  bool osc2 = (fdir == 1) ? (foto > luz2) : (foto < luz2);
  digitalWrite(PIN_LED1, (ledOverride ? osc1 : manLed1) ? HIGH : LOW);
  digitalWrite(PIN_LED2, (ledOverride ? osc2 : manLed2) ? HIGH : LOW);
  if (osc2 != oscActual) {
    oscActual = osc2;
    btPrintln("FOTO: " + String(foto) + (osc2 ? " -> OSCURO" : " -> claro"));
    enviar(osc2 ? "oscuro" : "claro", foto);
  }

  // F. Stream de sensores
  static unsigned long lastStream = 0;
  if (stream == 1 && millis() - lastStream >= 250) {
    lastStream = millis();
    char buf[100];
    sprintf(buf, "IR:%d FOTO:%d LED1:%d LED2:%d %s", ir, foto,
            (ledOverride ? osc1 : manLed1), (ledOverride ? osc2 : manLed2),
            obst ? "OBST" : "libre");
    Serial.println(buf);
    btNotificar(String(buf) + "\n");
  }

  // G. Cola BLE
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
  if (strncmp(m->tipo, "btn", 3) == 0) flagBtn = true;
  else if (strncmp(m->tipo, "ping", 4) == 0) flagPing = true;
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
// == SENSORES ==
// ==================================================
int leerIR() {
  long s = 0;
  for (int i = 0; i < nprom; i++) {
    s += analogRead(PIN_IR);
    delay(1);
  }
  return s / nprom;
}

int leerFoto() {
  long s = 0;
  for (int i = 0; i < nprom; i++) {
    s += analogRead(PIN_FOTO);
    delay(1);
  }
  return s / nprom;
}

// ==================================================
// == BUZZER ACTIVO ==
// ==================================================
void beep(int onMs) {
  digitalWrite(PIN_BUZZ, HIGH);
  delay(onMs);
  digitalWrite(PIN_BUZZ, LOW);
}

void sonarPatron(int veces, int onMs, int offMs) {
  for (int i = 0; i < veces; i++) {
    beep(onMs);
    if (i < veces - 1) delay(offMs);
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
  if (nombre == "buzz") {
    int ms = (valorStr.length() > 0) ? (int)valor : 500;
    btPrintln("Buzzer " + String(ms) + " ms");
    beep(ms);
    return;
  }
  if (nombre == "led1" || nombre == "led2") {
    int v = (valorStr.length() > 0 && valorStr.toInt() != 0) ? 1 : 0;
    if (nombre == "led1") manLed1 = v; else manLed2 = v;
    ledOverride = 0;
    digitalWrite((nombre == "led1") ? PIN_LED1 : PIN_LED2, v ? HIGH : LOW);
    btPrintln(nombre + " = " + String(v) + " (manual)");
    return;
  }
  if (nombre == "auto") {
    ledOverride = 1;
    btPrintln("LEDs en automatico (umbrales de la fotoresistencia)");
    return;
  }
  if (nombre == "ir") {
    btPrintln("IR: " + String(leerIR()));
    return;
  }
  if (nombre == "foto") {
    btPrintln("FOTO: " + String(leerFoto()));
    return;
  }
  if (nombre == "stream") {
    stream = (int)valor ? 1 : 0;
    btPrintln("stream = " + String(stream));
    return;
  }
  if (nombre == "simmuneca") {
    // Simula un mensaje entrante de la muñeca
    if (valorStr == "btn") { btPrintln("SIM muñeca: boton"); flagBtn = true; }
    else if (valorStr == "ping") { btPrintln("SIM muñeca: ping"); flagPing = true; }
    else btPrintln("Uso: simmuneca <btn|ping>");
    return;
  }
  if (nombre == "guardar") {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt("umbral", umbral);
    prefs.putInt("dirir", dirir);
    prefs.putInt("luz1", luz1);
    prefs.putInt("luz2", luz2);
    prefs.putInt("fdir", fdir);
    prefs.putInt("cdobs", cdobs);
    prefs.putInt("nprom", nprom);
    prefs.end();
    btPrintln("Guardado en flash.");
    return;
  }

  if (valorStr.length() == 0) {
    btPrintln("Falta el valor. Escribir 'help'.");
    return;
  }

  if (nombre == "umbral") umbral = constrain((int)valor, 0, 4095);
  else if (nombre == "dirir") dirir = (valor >= 0) ? 1 : -1;
  else if (nombre == "luz1") luz1 = constrain((int)valor, 0, 4095);
  else if (nombre == "luz2") luz2 = constrain((int)valor, 0, 4095);
  else if (nombre == "fdir") fdir = (valor >= 0) ? 1 : -1;
  else if (nombre == "cdobs") cdobs = constrain((int)valor, 0, 10000);
  else if (nombre == "nprom") nprom = constrain((int)valor, 1, 50);
  else {
    btPrintln("Comando no reconocido. Escribir 'help'.");
    return;
  }
  btPrintln(nombre + " = " + valorStr);
}

void mostrarAyuda() {
  btPrintln("ACCIONES: buzz led1 led2 auto ir foto stream simmuneca mac mostrar guardar help");
  btPrintln("simmuneca: <btn|ping> (simula mensaje de la muñeca)");
  btPrintln("IR: umbral dirir cdobs nprom | FOTO: luz1 luz2 fdir");
}

void mostrarValores() {
  btPrintln("===== VALORES =====");
  btPrintln("umbral=" + String(umbral) + " dirir=" + String(dirir) +
            " cdobs=" + String(cdobs) + " nprom=" + String(nprom));
  btPrintln("luz1=" + String(luz1) + " luz2=" + String(luz2) + " fdir=" + String(fdir));
  btPrintln("ledOverride=" + String(ledOverride) + " manLed1=" + String(manLed1) +
            " manLed2=" + String(manLed2) + " stream=" + String(stream));
  btPrintln("===================");
}
