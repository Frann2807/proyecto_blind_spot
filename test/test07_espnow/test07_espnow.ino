// ============================================================
// TEST 07 - ESP-NOW (ESP32-C3 Super Mini)
// - Inicializa ESP-NOW en broadcast (canal 1), el mismo esquema
//   que los firmwares finales.
// - Envia un "ping" cada 5 s y muestra el estado del envio.
// - Recibe y muestra cualquier mensaje (MAC emisora, tipo, valor).
// - Para probar el enlace real hacen falta DOS placas: flashear
//   las dos con este mismo test y se responden entre si (cada
//   una envia su propio ping cada 5 s).
// ============================================================

#include "espnow_cb.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

uint8_t macPeer[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
unsigned long lastPing = 0;

void OnDataSent(const uint8_t* mac, esp_now_send_status_t status) {
  (void)mac;
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "TX: entregado" : "TX: FALLO");
}

void onRecvMensaje(const uint8_t* mac, const Mensaje* m) {
  char strMac[18];
  snprintf(strMac, sizeof(strMac), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("RX de %s -> tipo='%s' valor=%d\n", strMac, m->tipo, m->valor);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("TEST07: ESP-NOW (broadcast, canal 1)");

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ERROR: esp_now_init fallo.");
    while (1) delay(1000);
  }
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(ESPNOW_RECV_CB);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, macPeer, 6);
  peer.channel = 1;
  peer.encrypt = false;
  esp_err_t r = esp_now_add_peer(&peer);
  Serial.println(r == ESP_OK ? "Peer broadcast agregado." : "Error agregando peer.");

  Serial.printf("MAC propia: %s\n", WiFi.macAddress().c_str());
  Serial.println("Envia 'ping' cada 5 s. Esperando mensajes...");
}

void loop() {
  if (millis() - lastPing >= 5000) {
    lastPing = millis();
    Mensaje m;
    strncpy(m.tipo, "ping", sizeof(m.tipo));
    m.valor = (int)(millis() / 1000);
    Serial.print("TX ping (broadcast) -> ");
    esp_now_send(macPeer, (uint8_t*)&m, sizeof(m));
  }
  delay(50);
}
