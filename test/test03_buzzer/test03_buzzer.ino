// ============================================================
// TEST 03 - BUZZER PASIVO (ESP32-C3 Super Mini)
// - Buzzer PASIVO en GPIO4 (modulo con transistor, o directo con
//   resistencia en serie). Se maneja con tone()/noTone() (LEDC),
//   igual que el firmware final del baston.
// - Melodia: Jingle Bells, en bucle.
// ============================================================
#define PIN_BUZZ 4

// Notas (frecuencias en Hz)
#define NOTE_C5 523
#define NOTE_D5 587
#define NOTE_E5 659
#define NOTE_F5 698
#define NOTE_G5 784
#define NOTE_R  0

int melodia[] = {
  NOTE_E5, NOTE_E5, NOTE_E5,
  NOTE_E5, NOTE_E5, NOTE_E5,
  NOTE_E5, NOTE_G5, NOTE_C5, NOTE_D5, NOTE_E5,
  NOTE_F5, NOTE_F5, NOTE_F5, NOTE_F5,
  NOTE_F5, NOTE_E5, NOTE_E5, NOTE_E5, NOTE_E5,
  NOTE_E5, NOTE_D5, NOTE_D5, NOTE_E5, NOTE_D5, NOTE_G5,
};

// 1 = redonda, 2 = blanca, 4 = negra, 8 = corchea, 16 = semicorchea
int duraciones[] = {
  4, 4, 2,
  4, 4, 2,
  4, 4, 4, 4, 1,
  4, 4, 4, 4,
  4, 4, 4, 8, 8,
  4, 4, 4, 4, 2, 2,
};

const int N_NOTAS = sizeof(melodia) / sizeof(melodia[0]);

void setup() {
  Serial.begin(115200);
  delay(1000);
  pinMode(PIN_BUZZ, OUTPUT);
  Serial.println("TEST03: BUZZER PASIVO (GPIO4)");
  Serial.println("Melodia: Jingle Bells (en bucle)");
}

void loop() {
  Serial.println("> Jingle Bells...");
  for (int i = 0; i < N_NOTAS; i++) {
    int durMs = 1600 / duraciones[i];  // negra = 400 ms (~150 BPM)
    if (melodia[i] != NOTE_R) {
      tone(PIN_BUZZ, melodia[i]);
    }
    delay((int)(durMs * 0.9));
    noTone(PIN_BUZZ);
    delay((int)(durMs * 0.1));
  }
  Serial.println("Fin de la melodia. Repitiendo en 2 s...");
  delay(2000);
}
