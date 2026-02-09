# Teclado de Jornada Digital v2.0

## What This Is

Firmware embarcado ESP32-S3 para um teclado touchscreen 3.5" que rastreia estados de jornada de motoristas profissionais (dirigindo, manobra, refeicao, espera, descarga, abastecimento) com feedback de audio e monitoramento de ignicao. Evolucao para arquitetura escalavel com suporte a multiplas telas, comunicacao BLE com app mobile, e OTA.

## Core Value

O sistema deve rastrear com confiabilidade os estados de jornada dos motoristas em tempo real, com interface responsiva e comunicacao segura via BLE — mesmo com perda de conexao, os dados de jornada nunca podem ser perdidos.

## Requirements

### Validated

<!-- Shipped and confirmed valuable — existing in v1.0/v1.1 -->

- ✓ Monitoramento de ignicao via GPIO18 com debounce duplo — v1.0
- ✓ Maquina de estados de jornada para ate 3 motoristas (7 estados) — v1.0
- ✓ Grid de botoes 4x3 com feedback de audio — v1.0
- ✓ Playback de audio MP3 via minimp3 + I2S (Core 1 dedicado) — v1.0
- ✓ Status bar com estado de ignicao, timers e mensagens — v1.0
- ✓ Splash screen PNG via LittleFS — v1.0
- ✓ Teclados de jornada e numpad — v1.0
- ✓ Arquitetura modular com interfaces abstratas (IIgnicaoService, IJornadaService, IAudioPlayer) — v1.1
- ✓ Thread safety com mutexes FreeRTOS — v1.0
- ✓ Configuracao centralizada em app_config.h — v1.0
- ✓ Tema centralizado (cores, fontes) — v1.1

### Active

<!-- Current scope — building toward these in v2.0 -->

- [ ] Framework de navegacao de telas generico (screen manager com stack de telas)
- [ ] Botao de menu na status bar para navegar entre telas
- [ ] Tela de configuracoes (volume, brilho, parametros do sistema)
- [ ] Tela de RPM e velocidade em tempo real
- [ ] Servico BLE (NimBLE) com pairing seguro e criptografia
- [ ] Protocolo BLE documentado para comunicacao com app mobile
- [ ] Envio de configuracoes via BLE (volume, brilho, motoristas)
- [ ] Recebimento de status via BLE (ignicao, jornada atual)
- [ ] Download de logs/historico de jornadas via BLE
- [ ] OTA (atualizacao de firmware) via BLE
- [ ] Refatoracao de button_manager.cpp (1205 linhas) para usar screen manager
- [ ] Particionamento flash para suportar OTA (dual app partitions)
- [ ] Persistencia de dados de jornada em NVS
- [ ] Documentacao completa do protocolo BLE para desenvolvimento do app

### Out of Scope

- App mobile (Android/iOS) — sera desenvolvido separadamente, depois do firmware
- Wi-Fi connectivity — nao necessario, BLE suficiente
- GPS/GNSS — sem hardware, fora do escopo
- Cloud connectivity — dispositivo opera offline
- Teclado fisico/botoes externos — somente touchscreen

## Context

- Hardware: ESP32-S3 dual-core 240MHz, 16MB flash, PSRAM, display 3.5" AXS15231B (480x320), touch capacitivo I2C, I2S audio
- Codebase existente: ~6500 linhas C/C++, arquitetura layered com services/interfaces/UI/HAL
- O ESP32-S3 tem BLE 5.0 nativo — NimBLE stack disponivel no ESP-IDF
- Particionamento atual: app0=3MB, spiffs=1MB, sem OTA — precisa reconfigurar para dual-app
- `src/ui/screens/` existe mas esta vazio — preparado para screen manager
- `button_manager.cpp` concentra toda a logica de UI em 1205 linhas — precisa ser decomposto
- O app mobile sera desenvolvido apos o firmware — protocolo BLE precisa estar muito bem documentado
- Seguranca BLE e requisito forte — pairing seguro, dados encriptados
- Idioma do codigo: portugues (BR) nos comentarios e termos de dominio

## Constraints

- **Hardware**: ESP32-S3 com 16MB flash, PSRAM — memoria limitada, precisa otimizar alocacoes
- **Flash**: Precisa caber firmware + OTA + filesystem em 16MB — particionamento cuidadoso
- **Real-time**: LVGL roda em Core 0 (5ms tick), audio em Core 1 — BLE nao pode bloquear nenhum
- **Seguranca**: BLE deve usar Secure Connections (LE Secure Connections, LESC) com encriptacao AES-CCM
- **Compatibilidade**: BLE deve funcionar com Android 8+ e iOS 13+ (BLE 4.2+)
- **Framework**: ESP-IDF 5.3.1 via PlatformIO (espressif32@6.9.0) — manter compatibilidade
- **Backward compatible**: Funcionalidades v1.x devem continuar funcionando apos refatoracao

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| NimBLE stack (nao Bluedroid) | Menor footprint de memoria, melhor para BLE-only, incluso no ESP-IDF | — Pending |
| Screen Manager com stack pattern | Permite push/pop de telas, historico de navegacao, facil de extender | — Pending |
| Menu na status bar (nao swipe) | Mais intuitivo para operadores de campo, evita conflito com swipe existente | — Pending |
| Protocolo BLE baseado em GATT services | Padrao BLE, facilita implementacao no app mobile | — Pending |
| OTA via BLE (nao Wi-Fi) | Dispositivo nao tem Wi-Fi configurado, BLE ja estara disponivel | — Pending |
| Dual app partitions para OTA | Permite rollback se OTA falhar — critico para dispositivos em campo | — Pending |

---
*Last updated: 2026-02-09 after initialization*
