// Provisiona o FIWARE: service group, dispositivos e assinatura do STH-Comet.
// Uso: npm run provision   (com o docker compose já rodando)
import {
  APIKEY, FIWARE_SERVICE, FIWARE_SERVICEPATH, ORION_URL, IOTA_URL, STH_URL_INTERNAL, DEVICES,
} from './config.js';

// O Orion recusa Content-Type em GET/DELETE, então só enviamos quando há corpo
const headers = {
  'fiware-service': FIWARE_SERVICE,
  'fiware-servicepath': FIWARE_SERVICEPATH,
};

async function call(method, url, body, okIfConflict = true) {
  const res = await fetch(url, {
    method,
    headers: body ? { ...headers, 'Content-Type': 'application/json' } : headers,
    body: body ? JSON.stringify(body) : undefined,
  });
  const text = await res.text();
  if (res.ok) return console.log(`  ✔ ${method} ${url} → ${res.status}`);
  if (okIfConflict && res.status === 409) return console.log(`  • ${method} ${url} → já existe`);
  throw new Error(`${method} ${url} → ${res.status} ${text}`);
}

async function main() {
  console.log('1) Verificando serviços...');
  await call('GET', `${ORION_URL}/version`);
  await call('GET', `${IOTA_URL}/iot/about`);

  console.log('2) Criando service group (apikey)...');
  await call('POST', `${IOTA_URL}/iot/services`, {
    services: [{
      apikey: APIKEY,
      cbroker: 'http://orion:1026',
      entity_type: 'Thing',
      resource: '',
    }],
  });

  console.log('3) Registrando dispositivos...');
  for (const d of DEVICES) {
    await call('POST', `${IOTA_URL}/iot/devices`, {
      devices: [{ ...d, protocol: 'PDI-IoTA-UltraLight', apikey: APIKEY, timezone: 'America/Sao_Paulo' }],
    });
  }

  console.log('4) Assinatura Orion → STH-Comet (histórico de luminosidade)...');
  const subsRes = await fetch(`${ORION_URL}/v2/subscriptions`, { headers });
  const subs = await subsRes.json();
  if (subs.some((s) => s.description === 'casinha-luminosity-sth')) {
    console.log('  • assinatura já existe');
  } else {
    await call('POST', `${ORION_URL}/v2/subscriptions`, {
      description: 'casinha-luminosity-sth',
      subject: {
        entities: [{ idPattern: '.*', type: 'LuminositySensor' }],
        condition: { attrs: ['luminosity'] },
      },
      notification: {
        http: { url: `${STH_URL_INTERNAL}/notify` },
        attrs: ['luminosity'],
        attrsFormat: 'legacy',
      },
    }, false);
  }

  console.log(`\nPronto! APIKEY = ${APIKEY}`);
  console.log('Tópicos MQTT:');
  for (const d of DEVICES) {
    console.log(`  /${APIKEY}/${d.device_id}/attrs  (publicar)` + (d.commands ? `   /${APIKEY}/${d.device_id}/cmd  (assinar)` : ''));
  }
}

main().catch((e) => { console.error('\n✖', e.message); process.exit(1); });
