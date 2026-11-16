#!/usr/bin/env bash
# Prepara uma VM Ubuntu nova: instala Docker e libera as portas 80/443.
# Uso: bash setup-vm.sh   (depois saia e entre de novo no SSH)
set -euo pipefail

if ! command -v docker >/dev/null; then
  curl -fsSL https://get.docker.com | sudo sh
  sudo usermod -aG docker "$USER"
fi

# Firewall do próprio Ubuntu (se estiver ativo)
if command -v ufw >/dev/null && sudo ufw status | grep -q active; then
  sudo ufw allow 22/tcp && sudo ufw allow 80/tcp && sudo ufw allow 443/tcp
fi

# Oracle Cloud: as imagens Ubuntu vêm com regras de iptables que bloqueiam tudo além do SSH
if sudo iptables -L INPUT -n 2>/dev/null | grep -q "REJECT"; then
  sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport 80 -j ACCEPT
  sudo iptables -I INPUT 6 -m state --state NEW -p tcp --dport 443 -j ACCEPT
  sudo netfilter-persistent save 2>/dev/null || true
fi

echo "Pronto. Saia e entre de novo no SSH para usar 'docker' sem sudo."
