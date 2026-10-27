// ============================================================
// TEST 02 - TRANSISTOR / MOTOR VIBRADOR (ESP32-C3 Super Mini)
// - GPIO10 controla el transistor (NPN/MOSFET) del vibrador.
// - Secuencia: 3 ciclos ON/OFF, rampa de PWM 0->255 y 255->0.
// - IMPORTANTE: se usa analogWrite TAMBIEN para ON/OFF (255/0).
//   En ESP32, mezclar digitalWrite con analogWrite (LEDC) hace
//   que digitalWrite deje de actuar sobre el pin.
// - Si el motor esta conectado se debe sentir la vibracion
//   (y mas intensa cuanto mayor el PWM).
// ============================================================
#define PIN_TRANS 10

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_TRANS, OUTPUT);
  analogWriteResolution(PIN_TRANS, 8);
  analogWrite(PIN_TRANS, 0);
  Serial.println("TEST02: TRANSISTOR / VIBRADOR (GPIO10)");
}

void loop() {
  Serial.println("> Fase 1: ON/OFF 3 ciclos (1 s cada uno)");
  for (int i = 0; i < 3; i++) {
    analogWrite(PIN_TRANS, 255);
    Serial.println("   ON");
    delay(1000);
    analogWrite(PIN_TRANS, 0);
    Serial.println("   OFF");
    delay(1000);
  }

  Serial.println("> Fase 2: rampa PWM 0 -> 255");
  for (int p = 0; p <= 255; p += 5) {
    analogWrite(PIN_TRANS, p);
    delay(30);
  }

  Serial.println("> Fase 3: rampa PWM 255 -> 0");
  for (int p = 255; p >= 0; p -= 5) {
    analogWrite(PIN_TRANS, p);
    delay(30);
  }
  analogWrite(PIN_TRANS, 0);

  Serial.println("Ciclo completo. Repitiendo en 2 s...");
  delay(2000);
}
