// ============================================================
// TEST 06 - LEDS DEL BASTON (ESP32-C3 Super Mini)
// - LED1 en GPIO5 y LED2 en GPIO6 (con resistencia ~220-330 ohm
//   hacia GND).
// - Secuencia: LED1, LED2, ambos, alternado y fundido PWM.
// - Se usa analogWrite (255/0) tambien para encender/apagar:
//   en ESP32, mezclar digitalWrite con analogWrite (LEDC) hace
//   que digitalWrite deje de actuar sobre el pin.
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
  analogWrite(PIN_LED1, 0);
  analogWrite(PIN_LED2, 0);
  Serial.println("TEST06: LEDS (GPIO5 y GPIO6)");
}

void loop() {
  Serial.println("> LED1 encendido");
  analogWrite(PIN_LED1, 255);
  delay(1000);
  analogWrite(PIN_LED1, 0);
  delay(300);

  Serial.println("> LED2 encendido");
  analogWrite(PIN_LED2, 255);
  delay(1000);
  analogWrite(PIN_LED2, 0);
  delay(300);

  Serial.println("> Ambos encendidos");
  analogWrite(PIN_LED1, 255);
  analogWrite(PIN_LED2, 255);
  delay(1000);
  analogWrite(PIN_LED1, 0);
  analogWrite(PIN_LED2, 0);
  delay(300);

  Serial.println("> Alternado 5 veces");
  for (int i = 0; i < 5; i++) {
    analogWrite(PIN_LED1, 255);
    analogWrite(PIN_LED2, 0);
    delay(200);
    analogWrite(PIN_LED1, 0);
    analogWrite(PIN_LED2, 255);
    delay(200);
  }
  analogWrite(PIN_LED1, 0);
  analogWrite(PIN_LED2, 0);

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
  analogWrite(PIN_LED1, 0);
  analogWrite(PIN_LED2, 0);

  Serial.println("Ciclo completo. Repitiendo en 2 s...");
  delay(2000);
}
