# Deploy (FIWARE + backend + HTTPS)

```
ESP32 ──MQTT──> broker.hivemq.com <──MQTT── IoT Agent ─┐
                                                       │  rede interna do Docker
Frontend (Vercel) ──HTTPS──> ngrok ou Caddy ──> backend ──> Orion / STH-Comet / Mongo
```

Orion, IoT Agent, STH e Mongo não ficam expostos na internet.
O ESP32 não muda nada: continua falando só com o broker MQTT público.
A porta de entrada é escolhida por `COMPOSE_PROFILES` no `.env`: **`pc`** (ngrok) ou **`vm`** (Caddy).

---

## Opção pc: no seu computador com ngrok

O site só funciona enquanto o PC estiver ligado e o Docker Desktop aberto.

### 1. Conta no ngrok (grátis, sem cartão)
1. Crie a conta em https://ngrok.com
2. Copie o **authtoken** em *Getting Started → Your Authtoken*.
3. Em *Domains*, pegue (ou crie) seu domínio fixo grátis, algo como `xxxx.ngrok-free.app`.

### 2. Configurar
No PowerShell, dentro de `CasinhaFiware\deploy`:
```powershell
Copy-Item .env.example .env
notepad .env     # COMPOSE_PROFILES=pc, NGROK_AUTHTOKEN, NGROK_URL e CORS_ORIGIN
```

### 3. Subir
Se o compose antigo de desenvolvimento (`CasinhaFiware\docker-compose.yml`) estiver rodando, desligue antes,
senão vão existir dois IoT Agents ouvindo o mesmo MQTT. O mesmo vale para um `npm start` do backend.
```powershell
cd ..;  docker compose down;  cd deploy
docker compose up -d --build
docker compose logs -f provision ngrok    # Ctrl+C para sair
```
O `provision` registra os dispositivos sozinho e termina com "Pronto!".

Teste: `https://SEU-DOMINIO.ngrok-free.app/api/health`. O ngrok mostra uma página de aviso no navegador;
clique em *Visit Site*. O frontend pula esse aviso sozinho.

### 4. Apontar o frontend
Na Vercel: **Settings → Environment Variables → `VITE_API_URL` = `https://SEU-DOMINIO.ngrok-free.app`** → **Redeploy**.
Como o domínio do ngrok é fixo, isso só precisa ser feito uma vez.

---

## Opção vm: numa VM com Caddy

1. VM Ubuntu x64 com **2 GB de RAM ou mais**, com TCP **22, 80 e 443** liberados no painel da nuvem. Anote o IP público e deixe-o estático.
2. Na VM:
   ```bash
   mkdir -p ~/casinha && cd ~/casinha
   git clone <url do repo CasinhaFiware>
   git clone <url do repo CasinhaBackend>
   bash CasinhaFiware/deploy/setup-vm.sh && exit   # entre de novo no SSH
   cd ~/casinha/CasinhaFiware/deploy && cp .env.example .env && nano .env   # COMPOSE_PROFILES=vm, DOMAIN, CORS_ORIGIN
   docker compose up -d --build
   ```
3. Na Vercel, `VITE_API_URL` = `https://SEU_DOMAIN` → Redeploy.

Se o HTTPS não sair, veja `docker compose logs caddy`. Erro de "challenge" quase sempre é porta 80/443 bloqueada.

---

## Comandos úteis
| O quê | Comando |
|---|---|
| Ver status | `docker compose ps` |
| Logs do backend | `docker compose logs -f backend` |
| Atualizar o backend após `git pull` | `docker compose up -d --build backend` |
| Reprovisionar | `docker compose run --rm provision` |
| Parar tudo | `docker compose down` (os dados ficam nos volumes) |
