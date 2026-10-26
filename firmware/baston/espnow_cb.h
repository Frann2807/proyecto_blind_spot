// ============================================================
// ESP-NOW: callback de recepcion compatible con arduino-esp32
// 2.x y 3.x (en 3.x cambio la firma del callback de recepcion).
// Aqui se adapta y se delega en onRecvMensaje(), definida en el
// .ino, para no hacer I/O dentro de la tarea de WiFi.
// ============================================================
#pragma once

#include <Arduino.h>
#include <esp_now.h>

typedef struct {
  char tipo[8];   // "btn", "obst", "libre", "oscuro", "claro", "ping"
  int  valor;     // dato del sensor (opcional)
} Mensaje;

// Handler implementado en el .ino
void onRecvMensaje(const uint8_t* mac, const Mensaje* m);

// Firma de arduino-esp32 2.x
void OnDataRecv_2x(const uint8_t* mac, const uint8_t* data, int len) {
  if (len == (int)sizeof(Mensaje)) {
    Mensaje m;
    memcpy(&m, data, sizeof(m));
    onRecvMensaje(mac, &m);
  }
}

// Firma de arduino-esp32 3.x (recibe esp_now_recv_info_t)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void OnDataRecv_3x(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  OnDataRecv_2x(info->src_addr, data, len);
}
#define ESPNOW_RECV_CB OnDataRecv_3x
#else
#define ESPNOW_RECV_CB OnDataRecv_2x
#endif
