# Deploy na VM (FIWARE + backend + HTTPS)

```
ESP32 ──MQTT──> broker.hivemq.com <──MQTT── IoT Agent ─┐
                                                       │  rede interna do Docker (VM)
Frontend (Vercel) ──HTTPS──> Caddy ──> backend ──> Orion / STH-Comet / Mongo
```

Só as portas **80 e 443** ficam abertas. Orion, IoT Agent, STH e Mongo não ficam expostos na internet.
O ESP32 não muda nada: ele continua falando só com o broker MQTT público.

## 1. Criar a VM
- Ubuntu 22.04 ou 24.04, **mínimo 2 GB de RAM** (Mongo + Orion + IoT Agent + STH ocupam ~1 GB).
- Opções gratuitas: Oracle Cloud Always Free (Ampere ARM), Azure for Students, GCP com créditos.
- No painel da nuvem, libere a entrada TCP **22, 80 e 443** (Security List / Network Security Group / firewall).
- Anote o **IP público**.

## 2. Preparar a VM
```bash
ssh ubuntu@SEU_IP
mkdir -p ~/casinha && cd ~/casinha
git clone <url do repo CasinhaFiware>
git clone <url do repo CasinhaBackend>
bash CasinhaFiware/deploy/setup-vm.sh
exit   # e entre de novo no SSH
```

## 3. Configurar e subir
```bash
cd ~/casinha/CasinhaFiware/deploy
cp .env.example .env
nano .env        # DOMAIN e CORS_ORIGIN
docker compose up -d --build
docker compose logs -f provision caddy   # Ctrl+C para sair
```
O `provision` registra os dispositivos sozinho e termina com "Pronto!".
O Caddy emite o certificado HTTPS no primeiro acesso (leva alguns segundos).

Teste: `https://SEU_DOMAIN/api/health` deve mostrar `backend`, `orion`, `iotAgent` e `sth` com `"ok": true`.

## 4. Apontar o frontend
Na Vercel: **Settings → Environment Variables → `VITE_API_URL` = `https://SEU_DOMAIN`** e faça um **Redeploy**
(variáveis `VITE_` entram no build, então sem redeploy nada muda).
Depois disso o serviço da Render pode ser desligado.

## Comandos úteis
| O quê | Comando |
|---|---|
| Ver status | `docker compose ps` |
| Logs do backend | `docker compose logs -f backend` |
| Atualizar o backend após `git pull` | `docker compose up -d --build backend` |
| Reprovisionar | `docker compose run --rm provision` |
| Parar tudo | `docker compose down` (os dados ficam nos volumes) |

## Se o HTTPS não sair
- `docker compose logs caddy`: erro de "challenge" quase sempre é porta 80/443 bloqueada no painel da nuvem ou no `iptables`.
- Confira se o `DOMAIN` resolve para o IP da VM: `ping SEU_DOMAIN`.
