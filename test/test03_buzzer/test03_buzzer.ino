// ============================================================
// TEST 03 - BUZZER ACTIVO (ESP32-C3 Super Mini)
// - Buzzer ACTIVO en GPIO4 (modulo con driver o transistor).
// - Prueba patrones: beep largo, 3 cortos y SOS (se repiten).
// ============================================================
#define PIN_BUZZ 4

void beep(int onMs) {
  digitalWrite(PIN_BUZZ, HIGH);
  delay(onMs);
  digitalWrite(PIN_BUZZ, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_BUZZ, LOW);
  Serial.println("TEST03: BUZZER ACTIVO (GPIO4)");
  Serial.println("Patrones: largo | 3 cortos | SOS (en bucle)");
}

void loop() {
  Serial.println("> Beep largo (800 ms)");
  beep(800);
  delay(1000);

  Serial.println("> 3 beeps cortos (150 ms)");
  for (int i = 0; i < 3; i++) {
    beep(150);
    delay(100);
  }
  delay(1000);

  Serial.println("> SOS (3 cortos, 3 largos, 3 cortos)");
  for (int i = 0; i < 3; i++) { beep(120); delay(120); }
  delay(300);
  for (int i = 0; i < 3; i++) { beep(400); delay(120); }
  delay(300);
  for (int i = 0; i < 3; i++) { beep(120); delay(120); }

  Serial.println("Ciclo completo. Repitiendo en 2 s...");
  delay(2000);
}
