/*
 * Casinha IoT - ESP32 + FIWARE (IoT Agent UltraLight via MQTT público)
 *
 * Bibliotecas (Arduino IDE > Gerenciador de Bibliotecas):
 *   - PubSubClient  (Nick O'Leary)
 *   - ESP32Servo    (Kevin Harrington)
 *
 * Ligações (ajuste os pinos abaixo se precisar):
 *   Sensor de luz (módulo LDR, saída DO)     -> GPIO 32   (VCC no 3V3)
 *   Sensor de chuva (FC-37 + YL-38, saída DO) -> GPIO 35  (VCC no 3V3)
 *   Luz 1 / Luz 2 / Luz 3 (LED ou relé)      -> GPIO 25 / 26 / 27
 *   Servo da janela                          -> GPIO 13
 *   Servo do varal                           -> GPIO 33
 *   Servos: vermelho no VIN (5 V da USB) e GND em comum com o ESP32.
 *
 * Tópicos (UltraLight 2.0):
 *   publica  /<APIKEY>/<device>/attrs    ex: "l|73", "r|1"  ou  "s|on"
 *   assina   /<APIKEY>/<device>/cmd      ex: "lamp001@on|"
 *   responde /<APIKEY>/<device>/cmdexe   ex: "lamp001@on|on"
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// WIFI_SSID e WIFI_PASSWORD ficam em secrets.h (fora do git).
// Copie secrets.example.h para secrets.h e preencha.
#include "secrets.h"

// ---------------- CONFIGURAÇÃO ----------------

const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 1883;

// Tem que ser IGUAL à APIKEY de fiwares/config.js
const char* APIKEY = "casinha-fiap-7k2q9";

const int PIN_LDR       = 32;
const int PIN_LAMPS[3]  = {25, 26, 27};
const int PIN_WINDOW    = 13;
const int PIN_CLOTHES   = 33;  // não use o GPIO 12: é pino de boot
const int PIN_RAIN      = 35;  // DO do sensor de chuva (GPIO 35 é só entrada; o módulo já tem pull-up)

// Sensor de chuva ligado? Coloque false se ele não estiver na maquete:
// com o pino solto a leitura fica aleatória e a casa acharia que está chovendo.
const bool HAS_RAIN_SENSOR = true;

// Ângulos dos servos (calibre na sua maquete)
const int WINDOW_CLOSED_ANGLE  = 0;
const int WINDOW_OPEN_ANGLE    = 90;
const int CLOTHES_RETRACTED_ANGLE = 0;
const int CLOTHES_EXTENDED_ANGLE  = 180;

// Tempo que cada servo leva para ir de uma ponta à outra (em milissegundos).
// Aumente para girar mais devagar. O movimento acelera e freia suavemente,
// o que evita picos de corrente (o servo não "perde força" no meio).
const unsigned long WINDOW_MOVE_MS  = 3000;  // 3 s
const unsigned long CLOTHES_MOVE_MS = 5000;  // 5 s

// Leituras do LDR para 0% e 100% (calibre: veja o valor cru no Serial Monitor)
const int LDR_DARK  = 0;     // valor no escuro (módulo LDR de 3 pinos? inverta: 4095)
const int LDR_LIGHT = 4095;  // valor com muita luz (módulo LDR de 3 pinos? inverta: 0)

// Módulo LDR com só 3 pinos (VCC, GND, DO)? Ele não tem saída analógica:
// coloque true e ligue o DO no GPIO 32. O painel vai mostrar 0% (escuro) ou 100% (claro).
// O ponto de virada é ajustado no parafusinho azul (potenciômetro) do módulo.
const bool LDR_DIGITAL_ONLY = true;

const unsigned long LUMINOSITY_INTERVAL_MS = 3000;
// Reenvia o estado de tudo de tempos em tempos (o painel usa isso como "sinal de vida")
const unsigned long HEARTBEAT_INTERVAL_MS = 30000;
// LED azul da placa: pisca a cada comando recebido
const int PIN_BUILTIN_LED = 2;
// ----------------------------------------------

const char* LAMP_IDS[3] = {"lamp001", "lamp002", "lamp003"};

WiFiMulti wifiMulti;
WiFiClient espClient;
PubSubClient mqtt(espClient);
Servo windowServo;
Servo clothesServo;

bool lampOn[3] = {false, false, false};
bool windowOpen = false;
bool clothesExtended = false;
int lastRain = -1;  // -1 = ainda não leu; 1 = chovendo; 0 = seco
unsigned long lastLuminosity = 0;
unsigned long lastHeartbeat = 0;
unsigned long ledOffAt = 0;

String topic(const char* device, const char* suffix) {
  return String("/") + APIKEY + "/" + device + "/" + suffix;
}

void publishState(const char* device, const char* state) {
  String payload = String("s|") + state;
  mqtt.publish(topic(device, "attrs").c_str(), payload.c_str());
}

void confirmCommand(const char* device, const String& cmd, const char* result) {
  String payload = String(device) + "@" + cmd + "|" + result;
  mqtt.publish(topic(device, "cmdexe").c_str(), payload.c_str());
}

// Move o servo suavemente em `durationMs`, com aceleração e frenagem (curva cosseno).
// Usa microssegundos para ter mais resolução que graus inteiros = movimento mais liso.
// Cada passo dura 20 ms, que é exatamente um ciclo do sinal do servo (50 Hz).
void moveServo(Servo& s, int fromAngle, int toAngle, unsigned long durationMs) {
  const int fromUs = map(fromAngle, 0, 180, 500, 2400);
  const int toUs = map(toAngle, 0, 180, 500, 2400);
  const int steps = max(1, (int)(durationMs / 20));
  for (int i = 1; i <= steps; i++) {
    float t = (float)i / steps;
    float eased = 0.5f - 0.5f * cosf(PI * t);  // começa devagar, acelera, freia no fim
    s.writeMicroseconds(fromUs + (int)((toUs - fromUs) * eased));
    delay(20);
  }
  s.writeMicroseconds(toUs);
}

// "r|1" = chovendo, "r|0" = seco
void publishRain(int raining) {
  String payload = String("r|") + raining;
  mqtt.publish(topic("rain001", "attrs").c_str(), payload.c_str());
}

void publishAllStates() {
  for (int i = 0; i < 3; i++) publishState(LAMP_IDS[i], lampOn[i] ? "on" : "off");
  publishState("window001", windowOpen ? "open" : "closed");
  publishState("clothesline001", clothesExtended ? "extended" : "retracted");
}

void onMessage(char* t, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("<= %s  %s\n", t, msg.c_str());

  // feedback visual: LED da placa acende por 150 ms
  digitalWrite(PIN_BUILTIN_LED, HIGH);
  ledOffAt = millis() + 150;

  int at = msg.indexOf('@');
  int bar = msg.indexOf('|');
  if (at < 0) return;
  String device = msg.substring(0, at);
  String cmd = msg.substring(at + 1, bar < 0 ? msg.length() : bar);

  // Lâmpadas
  for (int i = 0; i < 3; i++) {
    if (device == LAMP_IDS[i] && (cmd == "on" || cmd == "off")) {
      lampOn[i] = (cmd == "on");
      digitalWrite(PIN_LAMPS[i], lampOn[i] ? HIGH : LOW);
      const char* st = lampOn[i] ? "on" : "off";
      confirmCommand(LAMP_IDS[i], cmd, st);
      publishState(LAMP_IDS[i], st);
      return;
    }
  }

  // Janela
  if (device == "window001" && (cmd == "open" || cmd == "close")) {
    bool open = (cmd == "open");
    if (open != windowOpen) {
      moveServo(windowServo,
                windowOpen ? WINDOW_OPEN_ANGLE : WINDOW_CLOSED_ANGLE,
                open ? WINDOW_OPEN_ANGLE : WINDOW_CLOSED_ANGLE,
                WINDOW_MOVE_MS);
      windowOpen = open;
    }
    const char* st = windowOpen ? "open" : "closed";
    confirmCommand("window001", cmd, st);
    publishState("window001", st);
    return;
  }

  // Varal
  if (device == "clothesline001" && (cmd == "extend" || cmd == "retract")) {
    bool ext = (cmd == "extend");
    if (ext != clothesExtended) {
      moveServo(clothesServo,
                clothesExtended ? CLOTHES_EXTENDED_ANGLE : CLOTHES_RETRACTED_ANGLE,
                ext ? CLOTHES_EXTENDED_ANGLE : CLOTHES_RETRACTED_ANGLE,
                CLOTHES_MOVE_MS);
      clothesExtended = ext;
    }
    const char* st = clothesExtended ? "extended" : "retracted";
    confirmCommand("clothesline001", cmd, st);
    publishState("clothesline001", st);
    return;
  }
}

// Cadastra as redes de secrets.h. A segunda (hotspot) só entra se foi preenchida.
void setupWifiNetworks() {
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Rede 1: %s\n", WIFI_SSID);
  if (strlen(WIFI_SSID_2) > 0 && strcmp(WIFI_SSID_2, "NOME_DO_HOTSPOT") != 0) {
    wifiMulti.addAP(WIFI_SSID_2, WIFI_PASSWORD_2);
    Serial.printf("Rede 2: %s\n", WIFI_SSID_2);
  }
}

// Procura as redes cadastradas e conecta na que tiver sinal mais forte.
void connectWifi() {
  Serial.print("Conectando no WiFi");
  while (wifiMulti.run(5000) != WL_CONNECTED) {
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK: %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
}

void connectMqtt() {
  while (!mqtt.connected()) {
    String clientId = "casinha-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    Serial.printf("Conectando no MQTT %s...\n", MQTT_HOST);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("MQTT OK");
      for (int i = 0; i < 3; i++) mqtt.subscribe(topic(LAMP_IDS[i], "cmd").c_str());
      mqtt.subscribe(topic("window001", "cmd").c_str());
      mqtt.subscribe(topic("clothesline001", "cmd").c_str());
      publishAllStates();
    } else {
      Serial.printf("Falhou (rc=%d), tentando de novo em 2s\n", mqtt.state());
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 3; i++) {
    pinMode(PIN_LAMPS[i], OUTPUT);
    digitalWrite(PIN_LAMPS[i], LOW);
  }
  pinMode(PIN_BUILTIN_LED, OUTPUT);
  digitalWrite(PIN_BUILTIN_LED, LOW);
  analogReadResolution(12);
  // Módulo digital: pull-up interno deixa o sinal estável (GPIO 34-39 não têm pull-up; use 32 ou 33)
  if (LDR_DIGITAL_ONLY) pinMode(PIN_LDR, INPUT_PULLUP);
  if (HAS_RAIN_SENSOR) pinMode(PIN_RAIN, INPUT);

  windowServo.setPeriodHertz(50);
  clothesServo.setPeriodHertz(50);
  windowServo.attach(PIN_WINDOW, 500, 2400);
  clothesServo.attach(PIN_CLOTHES, 500, 2400);
  windowServo.write(WINDOW_CLOSED_ANGLE);
  clothesServo.write(CLOTHES_RETRACTED_ANGLE);

  setupWifiNetworks();
  connectWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setBufferSize(512);
  // o movimento lento do servo segura o loop por alguns segundos;
  // keepalive maior evita o broker achar que o ESP32 caiu
  mqtt.setKeepAlive(60);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  if (millis() - lastLuminosity >= LUMINOSITY_INTERVAL_MS) {
    lastLuminosity = millis();
    int raw, pct;
    if (LDR_DIGITAL_ONLY) {
      raw = digitalRead(PIN_LDR);          // DO do módulo: LOW = claro, HIGH = escuro
      pct = (raw == LOW) ? 100 : 0;
    } else {
      raw = analogRead(PIN_LDR);
      pct = constrain(map(raw, LDR_DARK, LDR_LIGHT, 0, 100), 0, 100);
    }
    String payload = String("l|") + pct;
    mqtt.publish(topic("ldr001", "attrs").c_str(), payload.c_str());
    Serial.printf("LDR cru=%d -> %d%%\n", raw, pct);
    if (HAS_RAIN_SENSOR && lastRain >= 0) publishRain(lastRain);  // repete junto com a luz
  }

  // Chuva: lê sempre e avisa na hora em que muda (com 300 ms de confirmação contra ruído)
  if (HAS_RAIN_SENSOR) {
    static int candidate = -1;
    static unsigned long candidateSince = 0;
    int now = (digitalRead(PIN_RAIN) == LOW) ? 1 : 0;  // DO do módulo: LOW = molhado
    if (now != candidate) {
      candidate = now;
      candidateSince = millis();
    } else if (now != lastRain && millis() - candidateSince >= 300) {
      lastRain = now;
      publishRain(lastRain);
      Serial.println(lastRain ? "Chuva: MOLHADO" : "Chuva: seco");
    }
  }

  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = millis();
    publishAllStates();
  }

  if (ledOffAt && millis() >= ledOffAt) {
    digitalWrite(PIN_BUILTIN_LED, LOW);
    ledOffAt = 0;
  }
}
