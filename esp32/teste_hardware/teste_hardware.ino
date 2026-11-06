/*
 * TESTE DE HARDWARE - Casinha IoT
 * Sem Wi-Fi e sem MQTT: serve só para conferir as ligações.
 *
 * O que ele faz, em ciclo:
 *   1. Acende os 3 LEDs, um de cada vez
 *   2. Move o servo da janela (GPIO 13): 0° -> 90° -> 0°
 *   3. Move o servo do varal  (GPIO 33): 0° -> 180° -> 0°
 *   4. Mostra no Serial Monitor (115200) a leitura do LDR (GPIO 34) e a
 *      tensão de 3V3 vista por outros pinos analógicos, para achar fio solto.
 *
 * Tampe e ilumine o LDR enquanto olha o Serial Monitor: o valor TEM que mudar.
 */
#include <ESP32Servo.h>

const int PIN_LDR = 34;
const int PIN_LAMPS[3] = {25, 26, 27};
const int PIN_WINDOW = 13;
const int PIN_CLOTHES = 33;

Servo windowServo;
Servo clothesServo;

void printLdr(const char* stage) {
  int raw = analogRead(PIN_LDR);
  int mv = analogReadMilliVolts(PIN_LDR);
  Serial.printf("[%s] LDR GPIO34: cru=%4d  tensao=%4d mV  ", stage, raw, mv);
  if (raw == 0) Serial.println("<- 0 V: o 3V3 nao chega no LDR (fio solto, trilho da protoboard ou LDR no pino errado)");
  else if (raw >= 4095) Serial.println("<- 3,3 V cravado: falta o resistor de 10k para o GND");
  else Serial.println("<- OK, tampe/ilumine e veja se muda");
}

void sweep(Servo& s, const char* name, int maxAngle) {
  Serial.printf("Servo %s: indo para %d graus...\n", name, maxAngle);
  for (int a = 0; a <= maxAngle; a += 2) { s.write(a); delay(15); }
  delay(400);
  Serial.printf("Servo %s: voltando para 0 graus...\n", name);
  for (int a = maxAngle; a >= 0; a -= 2) { s.write(a); delay(15); }
  delay(400);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n===== TESTE DE HARDWARE - Casinha IoT =====");

  for (int i = 0; i < 3; i++) pinMode(PIN_LAMPS[i], OUTPUT);
  analogReadResolution(12);

  windowServo.setPeriodHertz(50);
  clothesServo.setPeriodHertz(50);
  bool okW = windowServo.attach(PIN_WINDOW, 500, 2400);
  bool okC = clothesServo.attach(PIN_CLOTHES, 500, 2400);
  Serial.printf("attach servo janela (GPIO %d): %s\n", PIN_WINDOW, okW ? "OK" : "FALHOU");
  Serial.printf("attach servo varal  (GPIO %d): %s\n", PIN_CLOTHES, okC ? "OK" : "FALHOU");
}

void loop() {
  Serial.println("\n--- LEDs ---");
  for (int i = 0; i < 3; i++) {
    Serial.printf("Acendendo LED da Luz %d (GPIO %d)\n", i + 1, PIN_LAMPS[i]);
    digitalWrite(PIN_LAMPS[i], HIGH);
    delay(600);
    digitalWrite(PIN_LAMPS[i], LOW);
  }

  printLdr("antes dos servos");

  Serial.println("--- Servos ---");
  sweep(windowServo, "JANELA (GPIO 13)", 90);
  sweep(clothesServo, "VARAL (GPIO 33)", 180);

  // leitura contínua do LDR por 5 s
  Serial.println("--- LDR por 5 s: tampe e ilumine agora ---");
  for (int i = 0; i < 10; i++) {
    printLdr("LDR");
    delay(500);
  }
}
