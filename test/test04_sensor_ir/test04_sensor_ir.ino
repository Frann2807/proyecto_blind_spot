// ============================================================
// TEST 04 - SENSOR INFRARROJO ANALOGICO (ESP32-C3 Super Mini)
// - Sensor IR analogico en GPIO0 (ADC1_CH0), alimentado a 3V3.
// - El sensor es PULL-UP: sin obstaculo la salida queda ALTA
//   (lectura alta, ~4095) y baja al acercarse un obstaculo.
//   Por eso el default es dir = -1 (obstaculo si lectura < umbral).
// - Muestra lectura cruda (0-4095) y mV cada 200 ms y marca
//   OBSTACULO segun umbral y direccion configurables por Serial.
// - Comandos: 'u<valor>' umbral | 'd1' / 'd-1' direccion | help
// ============================================================
#define PIN_IR 0
#define N_MUESTRAS 10

int umbral = 3800;  // default sensible: < 3800 ya es obstaculo
int dir = -1;       // sensor pull-up: obstaculo si lectura < umbral

void setup() {
  Serial.begin(115200);
  delay(1000);
  analogReadResolution(12);
  Serial.println("TEST04: SENSOR IR (GPIO0)");
  Serial.println("Comandos: u<valor> umbral | d1 / d-1 direccion | help");
}

int leerIR() {
  long s = 0;
  for (int i = 0; i < N_MUESTRAS; i++) {
    s += analogRead(PIN_IR);
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
        Serial.println("Dir = 1 (obstaculo si lectura > umbral)");
      } else if (in == "d-1") {
        dir = -1;
        Serial.println("Dir = -1 (obstaculo si lectura < umbral)");
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
  int raw = leerIR();
  bool obst = (dir == 1) ? (raw > umbral) : (raw < umbral);
  Serial.println("IR: " + String(raw) + " (" + String(raw * 3300.0f / 4095.0f, 0) + " mV) " +
                 (obst ? ">>> OBSTACULO" : "libre"));
  procesarSerial();
  delay(150);
}
