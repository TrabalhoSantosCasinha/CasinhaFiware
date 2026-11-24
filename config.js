// Configuração compartilhada entre provisionamento e simulador.
// IMPORTANTE: o broker é PÚBLICO — troque a APIKEY por algo único do seu grupo
// para não colidir com tópicos de outras pessoas. Use o mesmo valor no ESP32.
export const APIKEY = process.env.APIKEY || 'casinha-fiap-7k2q9';
export const FIWARE_SERVICE = 'casinha';
export const FIWARE_SERVICEPATH = '/';

export const ORION_URL = process.env.ORION_URL || 'http://localhost:1026';
export const IOTA_URL = process.env.IOTA_URL || 'http://localhost:4041';
export const STH_URL_INTERNAL = 'http://sth-comet:8666'; // visto de dentro da rede docker
export const MQTT_URL = process.env.MQTT_URL || 'mqtt://broker.hivemq.com:1883';

const stateAttr = [{ object_id: 's', name: 'state', type: 'Text' }];
const cmds = (...names) => names.map((name) => ({ name, type: 'command' }));

export const DEVICES = [
  {
    device_id: 'ldr001',
    entity_name: 'urn:ngsi-ld:LuminositySensor:001',
    entity_type: 'LuminositySensor',
    transport: 'MQTT',
    attributes: [{ object_id: 'l', name: 'luminosity', type: 'Integer' }],
  },
  {
    device_id: 'rain001',
    entity_name: 'urn:ngsi-ld:RainSensor:001',
    entity_type: 'RainSensor',
    transport: 'MQTT',
    attributes: [{ object_id: 'r', name: 'raining', type: 'Integer' }], // 1 = chovendo, 0 = seco
  },
  ...['001', '002', '003'].map((n) => ({
    device_id: `lamp${n}`,
    entity_name: `urn:ngsi-ld:Lamp:${n}`,
    entity_type: 'Lamp',
    transport: 'MQTT',
    commands: cmds('on', 'off'),
    attributes: stateAttr,
  })),
  {
    device_id: 'window001',
    entity_name: 'urn:ngsi-ld:Window:001',
    entity_type: 'Window',
    transport: 'MQTT',
    commands: cmds('open', 'close'),
    attributes: stateAttr,
  },
  {
    device_id: 'clothesline001',
    entity_name: 'urn:ngsi-ld:Clothesline:001',
    entity_type: 'Clothesline',
    transport: 'MQTT',
    commands: cmds('extend', 'retract'),
    attributes: stateAttr,
  },
];
