// Simula o ESP32 (sensor de luz, 3 lâmpadas, servo da janela e servo do varal)
// para testar sem hardware. Uso: npm run simulate
import mqtt from 'mqtt';
import { APIKEY, MQTT_URL, DEVICES } from './config.js';

const client = mqtt.connect(MQTT_URL, { clientId: `casinha-sim-${Math.random().toString(16).slice(2, 8)}` });

// estado inicial
const state = {
  lamp001: 'off',
  lamp002: 'off',
  lamp003: 'off',
  window001: 'closed',
  clothesline001: 'retracted',
};

const cmdToState = {
  on: 'on',
  off: 'off',
  open: 'open',
  close: 'closed',
  extend: 'extended',
  retract: 'retracted',
};

const attrsTopic = (id) => `/${APIKEY}/${id}/attrs`;
const publishState = (id) => client.publish(attrsTopic(id), `s|${state[id]}`);

client.on('connect', () => {
  console.log(`Conectado em ${MQTT_URL} (apikey ${APIKEY})`);
  for (const d of DEVICES.filter((dev) => dev.commands)) {
    client.subscribe(`/${APIKEY}/${d.device_id}/cmd`);
    publishState(d.device_id);
  }
});

// Payload UltraLight de comando: "<device>@<comando>|<valor>"
client.on('message', (topic, buf) => {
  const msg = buf.toString();
  const [device, rest] = msg.split('@');
  const cmd = (rest || '').split('|')[0];
  console.log(`<= ${topic}  ${msg}`);
  if (!(device in state) || !(cmd in cmdToState)) return;

  const delay = { window001: 3000, clothesline001: 5000 }[device] ?? 100; // igual ao ESP32
  setTimeout(() => {
    state[device] = cmdToState[cmd];
    // confirma o comando para o IoT Agent e publica o novo estado
    client.publish(`/${APIKEY}/${device}/cmdexe`, `${device}@${cmd}|${state[device]}`);
    publishState(device);
    console.log(`=> ${device} agora está ${state[device]}`);
  }, delay);
});

// heartbeat: reenvia o estado de tudo a cada 30 s (igual ao ESP32)
setInterval(() => {
  for (const id of Object.keys(state)) publishState(id);
}, 30000);

// luminosidade "realista": senoide + ruído, 0–100 %
let t = 0;
setInterval(() => {
  t += 0.05;
  const lum = Math.max(0, Math.min(100, Math.round(50 + 40 * Math.sin(t) + (Math.random() * 10 - 5))));
  client.publish(attrsTopic('ldr001'), `l|${lum}`);
}, 3000);
