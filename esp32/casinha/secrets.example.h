// Copie este arquivo para "secrets.h" (mesma pasta) e coloque os dados do seu Wi-Fi.
// O secrets.h está no .gitignore para a senha não ir para o GitHub.
#pragma once

// Rede principal (casa)
const char* WIFI_SSID     = "NOME_DA_REDE";
const char* WIFI_PASSWORD = "SENHA_DA_REDE";

// Segunda rede (hotspot do celular, para a apresentação na faculdade).
// O ESP32 tenta as duas e conecta na que estiver disponível.
// Se não for usar, deixe como está.
const char* WIFI_SSID_2     = "NOME_DO_HOTSPOT";
const char* WIFI_PASSWORD_2 = "SENHA_DO_HOTSPOT";
