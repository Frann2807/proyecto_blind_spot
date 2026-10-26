// ============================================================
// TEST 06 - LEDS DEL BASTON (ESP32-C3 Super Mini)
// - LED1 en GPIO5 y LED2 en GPIO6 (con resistencia ~220-330 ohm
//   hacia GND).
// - Secuencia: LED1, LED2, ambos, alternado y fundido PWM.
// ============================================================
#define PIN_LED1 5
#define PIN_LED2 6

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);
  analogWriteResolution(PIN_LED1, 8);
  analogWriteResolution(PIN_LED2, 8);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  Serial.println("TEST06: LEDS (GPIO5 y GPIO6)");
}

void loop() {
  Serial.println("> LED1 encendido");
  digitalWrite(PIN_LED1, HIGH);
  delay(1000);
  digitalWrite(PIN_LED1, LOW);
  delay(300);

  Serial.println("> LED2 encendido");
  digitalWrite(PIN_LED2, HIGH);
  delay(1000);
  digitalWrite(PIN_LED2, LOW);
  delay(300);

  Serial.println("> Ambos encendidos");
  digitalWrite(PIN_LED1, HIGH);
  digitalWrite(PIN_LED2, HIGH);
  delay(1000);
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);
  delay(300);

  Serial.println("> Alternado 5 veces");
  for (int i = 0; i < 5; i++) {
    digitalWrite(PIN_LED1, HIGH);
    digitalWrite(PIN_LED2, LOW);
    delay(200);
    digitalWrite(PIN_LED1, LOW);
    digitalWrite(PIN_LED2, HIGH);
    delay(200);
  }
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);

  Serial.println("> Fundido PWM (ambos)");
  for (int p = 0; p <= 255; p += 5) {
    analogWrite(PIN_LED1, p);
    analogWrite(PIN_LED2, p);
    delay(15);
  }
  for (int p = 255; p >= 0; p -= 5) {
    analogWrite(PIN_LED1, p);
    analogWrite(PIN_LED2, p);
    delay(15);
  }
  digitalWrite(PIN_LED1, LOW);
  digitalWrite(PIN_LED2, LOW);

  Serial.println("Ciclo completo. Repitiendo en 2 s...");
  delay(2000);
}
