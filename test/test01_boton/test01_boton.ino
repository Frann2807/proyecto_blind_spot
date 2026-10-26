// ============================================================
// TEST 01 - PULSADOR DE LA MUÑECA (ESP32-C3 Super Mini)
// - GPIO3 en INPUT_PULLDOWN: boton entre 3V3 y GPIO3.
// - Muestra por Serial (115200): flancos (con debounce de 50 ms,
//   igual que el firmware final) y conteo de pulsaciones.
// - El C3 tiene pulldown interno en todos los GPIO.
// ============================================================
#define PIN_BOTON 3
#define DEBOUNCE_MS 50

bool estadoEstable = LOW;
unsigned long tUltimoCambio = 0;
unsigned long ultimoReporte = 0;
int pulsaciones = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_BOTON, INPUT_PULLDOWN);
  Serial.println("TEST01: PULSADOR (GPIO3, pulldown interno)");
  Serial.println("Boton entre 3V3 y GPIO3. Suelto=LOW, Presionado=HIGH");
}

void loop() {
  bool lec = digitalRead(PIN_BOTON);
  if (lec != estadoEstable) {
    if (millis() - tUltimoCambio >= DEBOUNCE_MS) {
      estadoEstable = lec;
      tUltimoCambio = millis();
      if (lec == HIGH) {
        pulsaciones++;
        Serial.println("FLANCO: PRESIONADO (pulsacion N° " + String(pulsaciones) + ")");
      } else {
        Serial.println("FLANCO: SOLTADO");
      }
    }
  }

  if (millis() - ultimoReporte >= 1000) {
    ultimoReporte = millis();
    Serial.println("Estado: " + String(estadoEstable == HIGH ? "PRESIONADO" : "suelto") +
                   " | pulsaciones totales: " + String(pulsaciones));
  }
  delay(10);
}
