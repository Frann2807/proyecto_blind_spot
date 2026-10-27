// ============================================================
// TEST 01 - PULSADOR DE LA MUÑECA (ESP32-C3 Super Mini)
// - GPIO3 en INPUT_PULLUP: boton entre GPIO3 y GND.
// - Muestra por Serial (115200): flancos (con debounce de 50 ms,
//   igual que el firmware final) y conteo de pulsaciones.
// ============================================================
#define PIN_BOTON 3
#define DEBOUNCE_MS 50

bool estadoEstable = HIGH;
unsigned long tUltimoCambio = 0;
unsigned long ultimoReporte = 0;
int pulsaciones = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_BOTON, INPUT_PULLUP);
  Serial.println("TEST01: PULSADOR (GPIO3, pullup interno)");
  Serial.println("Boton entre GPIO3 y GND. Suelto=HIGH, Presionado=LOW");
}

void loop() {
  bool lec = digitalRead(PIN_BOTON);
  if (lec != estadoEstable) {
    if (millis() - tUltimoCambio >= DEBOUNCE_MS) {
      estadoEstable = lec;
      tUltimoCambio = millis();
      if (lec == LOW) {
        pulsaciones++;
        Serial.println("FLANCO: PRESIONADO (pulsacion N° " + String(pulsaciones) + ")");
      } else {
        Serial.println("FLANCO: SOLTADO");
      }
    }
  }

  if (millis() - ultimoReporte >= 1000) {
    ultimoReporte = millis();
    Serial.println("Estado: " + String(estadoEstable == LOW ? "PRESIONADO" : "suelto") +
                   " | pulsaciones totales: " + String(pulsaciones));
  }
  delay(10);
}
