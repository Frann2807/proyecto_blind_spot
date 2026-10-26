// ============================================================
// TEST 05 - FOTORRESISTENCIA (ESP32-C3 Super Mini)
// - Fotoresistencia (LDR) con divisor resistivo en GPIO1
//   (ADC1_CH1), alimentado a 3V3.
// - Muestra lectura cruda (0-4095) y mV cada 200 ms y marca
//   OSCURO segun umbral y direccion configurables por Serial.
// - Comandos: 'u<valor>' umbral | 'd1' / 'd-1' direccion | help
//   (1 = oscuro si lectura > umbral, -1 = si lectura < umbral;
//    depende de como armes el divisor)
// ============================================================
#define PIN_FOTO 1
#define N_MUESTRAS 10

int umbral = 2000;
int dir = 1;

void setup() {
  Serial.begin(115200);
  delay(1000);
  analogReadResolution(12);
  Serial.println("TEST05: FOTORRESISTENCIA (GPIO1)");
  Serial.println("Comandos: u<valor> umbral | d1 / d-1 direccion | help");
}

int leerFoto() {
  long s = 0;
  for (int i = 0; i < N_MUESTRAS; i++) {
    s += analogRead(PIN_FOTO);
    delay(1);
  }
  return s / N_MUESTRAS;
}

void procesarSerial() {
  static String in = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      in.trim();
      in.toLowerCase();
      if (in == "help") {
        Serial.println("u<valor> = umbral | d1 / d-1 = direccion | help");
      } else if (in.startsWith("u")) {
        umbral = in.substring(1).toInt();
        Serial.println("Umbral = " + String(umbral));
      } else if (in == "d1") {
        dir = 1;
        Serial.println("Dir = 1 (oscuro si lectura > umbral)");
      } else if (in == "d-1") {
        dir = -1;
        Serial.println("Dir = -1 (oscuro si lectura < umbral)");
      } else if (in.length() > 0) {
        Serial.println("Comando no reconocido. Escribir 'help'.");
      }
      in = "";
    } else {
      in += c;
    }
  }
}

void loop() {
  int raw = leerFoto();
  bool osc = (dir == 1) ? (raw > umbral) : (raw < umbral);
  Serial.println("FOTO: " + String(raw) + " (" + String(raw * 3300.0f / 4095.0f, 0) + " mV) " +
                 (osc ? ">>> OSCURO" : "iluminado"));
  procesarSerial();
  delay(150);
}
