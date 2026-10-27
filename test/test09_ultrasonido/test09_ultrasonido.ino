// ============================================================
// TEST 09 - SENSOR ULTRASONIDO HC-SR04 (ESP32-C3 Super Mini)
// Plan B vs el IR: comparar a que distancia detecta cada uno.
//
// - TRIG en GPIO20 (salida), ECHO en GPIO21 (entrada).
//   ATENCION: el ECHO sale a la tension de VCC del sensor. Con
//   bateria de litio (3.7-4.2V) va divisor: R1 = 1k entre ECHO y
//   GPIO21 (en serie), R2 = 3k entre GPIO21 y GND. Resultado:
//   4.2V -> 3.15V | 3.7V -> 2.78V (seguro y por encima del VIH).
//   El VCC del HC-SR04 va directo al + de la bateria (a 3.7-4.2V
//   funciona, con algo menos de alcance que a 5V).
// - Filtros de ruido:
//     * sin eco / fuera de rango (> 400 cm): lectura 0, ignorada
//     * muy cerca (< 2 cm): zona ciega del sensor, ignorada
//     * mediana de 5 muestras (descarta picos sueltos)
//     * el estado (OBSTACULO/libre) cambia recien tras 2
//       lecturas consecutivas iguales (debounce)
// - Muestra por Serial: distancia, estado y maxima distancia
//   detectada en la sesion (para comparar con el IR).
// - Comandos: 'u<cm>' umbral de obstaculo (default 50) | help
// ============================================================

#define PIN_TRIG 20
#define PIN_ECHO 21

#define N_MUESTRAS 5
#define MIN_CM 2.0f      // zona ciega: mas cerca que esto = ruido
#define MAX_CM 400.0f    // mas lejos que esto no es confiable

int umbralCm = 50;   // distancia (cm) que dispara OBSTACULO
float maxCm = 0;     // maxima distancia confiable de la sesion

enum { EST_LIBRE, EST_OBSTACULO };
int estado = EST_LIBRE;
int contador = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  Serial.println("TEST09: ULTRASONIDO HC-SR04 (TRIG=GPIO20, ECHO=GPIO21)");
  Serial.println("Comandos: u<cm> umbral de obstaculo | help");
}

float medirCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long t = pulseIn(PIN_ECHO, HIGH, 30000UL);  // timeout 30 ms (~5 m)
  if (t == 0) return 0;
  return t * 0.0343f / 2.0f;
}

float mediana() {
  float muestras[N_MUESTRAS];
  for (int i = 0; i < N_MUESTRAS; i++) {
    muestras[i] = medirCm();
    delay(50);  // ciclo recomendado del HC-SR04
  }
  for (int i = 0; i < N_MUESTRAS - 1; i++) {
    for (int j = i + 1; j < N_MUESTRAS; j++) {
      if (muestras[j] < muestras[i]) {
        float t = muestras[i];
        muestras[i] = muestras[j];
        muestras[j] = t;
      }
    }
  }
  return muestras[N_MUESTRAS / 2];
}

void procesarSerial() {
  static String in = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      in.trim();
      in.toLowerCase();
      if (in == "help") {
        Serial.println("u<cm> = umbral de obstaculo | help");
      } else if (in.startsWith("u")) {
        umbralCm = in.substring(1).toInt();
        if (umbralCm < 1) umbralCm = 1;
        Serial.println("Umbral = " + String(umbralCm) + " cm");
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
  float d = mediana();

  // Clasificacion con filtros
  String etiqueta;
  bool valida = false;
  if (d <= 0.0f || d > MAX_CM) {
    etiqueta = "SIN ECO / FUERA DE RANGO";
  } else if (d < MIN_CM) {
    etiqueta = "MUY CERCA (ruido)";
  } else {
    valida = true;
    if (d > maxCm) maxCm = d;
    etiqueta = (d <= umbralCm) ? ">>> OBSTACULO" : "libre";
  }

  // Debounce del estado (solo con lecturas validas)
  if (valida) {
    int nuevo = (d <= umbralCm) ? EST_OBSTACULO : EST_LIBRE;
    if (nuevo == estado) {
      contador = 0;
    } else {
      contador++;
      if (contador >= 2) {
        estado = nuevo;
        contador = 0;
        Serial.println("CAMBIO: " + String(estado == EST_OBSTACULO ? "OBSTACULO" : "libre"));
      }
    }
  }

  Serial.println("US: " + String(d, 1) + " cm (" + etiqueta + ") | umbral=" +
                 String(umbralCm) + " cm | max sesion: " + String(maxCm, 1) + " cm");
  procesarSerial();
  delay(100);
}
