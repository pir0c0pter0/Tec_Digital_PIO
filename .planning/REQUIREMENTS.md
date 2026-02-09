# Requirements: Teclado de Jornada Digital v2.0

**Defined:** 2026-02-09
**Core Value:** Rastrear estados de jornada de motoristas em tempo real com interface responsiva, comunicacao segura via BLE, atualizacoes OTA, e arquitetura escalavel para multiplas telas.

## v1 Requirements

Requirements para o milestone v2.0. Cada um mapeia para fases do roadmap.

### Infraestrutura (INFRA)

- [ ] **INFRA-01**: Tabela de particoes reconfigurada para dual OTA (ota_0=3MB, ota_1=3MB, spiffs=1MB, otadata=8KB, nvs_data=64KB)
- [ ] **INFRA-02**: sdkconfig atualizado com flags NimBLE (BT_ENABLED, BT_NIMBLE_ENABLED, SM_SC, NVS_PERSIST)
- [ ] **INFRA-03**: sdkconfig atualizado com flag de rollback OTA (BOOTLOADER_APP_ROLLBACK_ENABLE)
- [ ] **INFRA-04**: LVGL widgets habilitados em lv_conf.h (LV_USE_SLIDER=1, LV_USE_METER=1, LV_USE_ARC=1)
- [ ] **INFRA-05**: Constantes BLE, OTA e NVS centralizadas em app_config.h

### Screen Manager (SCR)

- [ ] **SCR-01**: ScreenManager implementado com stack de navegacao (push/pop) usando interface IScreenManager existente
- [ ] **SCR-02**: Transicoes de tela com animacao via lv_scr_load_anim() (slide left/right, 200-300ms)
- [ ] **SCR-03**: StatusBar persistente em lv_layer_top() visivel em todas as telas
- [ ] **SCR-04**: Botao de menu na StatusBar para navegar entre telas
- [ ] **SCR-05**: Botao de voltar (back) em telas nao-home para retornar via pop do stack
- [ ] **SCR-06**: Screen lifecycle completo: create(), onEnter(), onExit(), destroy(), update()
- [ ] **SCR-07**: JornadaScreen extraida de button_manager.cpp como IScreen
- [ ] **SCR-08**: NumpadScreen extraida de numpad_example.cpp como IScreen

### Persistencia NVS (NVS)

- [ ] **NVS-01**: NvsManager com namespace separado (nvs_data partition) para dados da aplicacao
- [ ] **NVS-02**: Persistencia de configuracoes (volume, brilho) com restauracao no boot
- [ ] **NVS-03**: Persistencia de estado de jornada por motorista com restauracao no boot
- [ ] **NVS-04**: Persistencia de nomes de motoristas configurados via BLE

### BLE Core (BLE)

- [ ] **BLE-01**: Stack NimBLE inicializado como GATT Server (peripheral only, 1 conexao)
- [ ] **BLE-02**: Advertising com nome do dispositivo ("GS-Jornada-XXXX" com ultimos 4 do MAC) e UUIDs de servico
- [ ] **BLE-03**: LE Secure Connections (LESC) com encriptacao AES-CCM
- [ ] **BLE-04**: Bonding persistido em NVS com auto-reconnect a dispositivo pareado
- [ ] **BLE-05**: Negociacao de MTU (request 512 bytes) para transferencia eficiente
- [ ] **BLE-06**: Icone de status BLE na StatusBar (desconectado/advertising/conectado/seguro)
- [ ] **BLE-07**: Event queue (FreeRTOS) para comunicacao BLE-to-UI sem violar thread safety do LVGL
- [ ] **BLE-08**: Versao de protocolo em Device Info characteristic para detectar incompatibilidade app/firmware

### GATT Services (GATT)

- [ ] **GATT-01**: Device Information Service (SIG 0x180A) com manufacturer, model, firmware version, hardware revision
- [ ] **GATT-02**: Journey Service (custom UUID) com estado atual por motorista (read + notify)
- [ ] **GATT-03**: Journey Service com tempo no estado atual por motorista (read + notify)
- [ ] **GATT-04**: Journey Service com status de ignicao (read + notify) e duracao
- [ ] **GATT-05**: Configuration Service (custom UUID) com volume, brilho (read + write)
- [ ] **GATT-06**: Configuration Service com nomes de motoristas (read + write, 32 bytes UTF-8)
- [ ] **GATT-07**: Configuration Service com system time sync (write, Unix timestamp 4 bytes)
- [ ] **GATT-08**: Diagnostics Service (custom UUID) com free heap, uptime, BLE connections, audio queue depth
- [ ] **GATT-09**: Validacao server-side de todos os writes BLE (bounds checking, tipo, tamanho)

### Tela de Configuracoes (CFG)

- [ ] **CFG-01**: SettingsScreen com slider de volume (0-21) funcional
- [ ] **CFG-02**: SettingsScreen com slider de brilho (0-100%) funcional com ajuste de backlight
- [ ] **CFG-03**: SettingsScreen com informacoes do sistema (versao firmware, uptime, memoria livre)
- [ ] **CFG-04**: Alteracoes de configuracao persistidas em NVS imediatamente
- [ ] **CFG-05**: Sincronizacao bidirecional: mudancas via tela refletem no BLE e vice-versa

### Tela RPM/Velocidade (RPM)

- [ ] **RPM-01**: RPMScreen com gauge de RPM usando lv_meter (visual completo, dados placeholder)
- [ ] **RPM-02**: RPMScreen com gauge de velocidade usando lv_meter (visual completo, dados placeholder)
- [ ] **RPM-03**: Infraestrutura para receber dados RPM/velocidade via BLE characteristic (read + write from external adapter)
- [ ] **RPM-04**: Limiares de alerta RPM/velocidade configuraveis via BLE e persistidos em NVS

### OTA (OTA)

- [ ] **OTA-01**: OTA Service GATT (custom UUID) com control, data, status, image size, image hash characteristics
- [ ] **OTA-02**: Transferencia de firmware via BLE com chunks write-without-response (tamanho MTU-3)
- [ ] **OTA-03**: Verificacao de integridade SHA-256 antes de commitar imagem OTA
- [ ] **OTA-04**: Rollback automatico se novo firmware nao completar self-test em 60 segundos (watchdog)
- [ ] **OTA-05**: OtaScreen com progress bar mostrando porcentagem, bytes recebidos, tempo estimado
- [ ] **OTA-06**: Maquina de estados OTA: IDLE -> PREPARING -> RECEIVING -> VERIFYING -> REBOOTING (+ ABORTING, FAILED)
- [ ] **OTA-07**: Self-test pos-OTA que valida BLE init, LVGL init, touch responsive antes de marcar firmware como valido
- [ ] **OTA-08**: UI bloqueada durante OTA para prevenir interrupcao acidental

### Documentacao (DOC)

- [ ] **DOC-01**: Protocolo BLE documentado: todos os UUIDs, formatos de dados, byte order, ranges validos, frequencia de notificacao
- [ ] **DOC-02**: Documentacao inclui versao do protocolo e guia de compatibilidade para equipe do app mobile
- [ ] **DOC-03**: Documentacao do fluxo OTA: sequencia de operacoes, tratamento de erros, limites de MTU

## v2 Requirements

Reconhecidos mas deferidos para milestone futuro. Nao estao no roadmap atual.

### Historico de Jornada

- **HIST-01**: Armazenamento circular de historico de transicoes de jornada em LittleFS (timestamp, driver_id, state_from, state_to, duration)
- **HIST-02**: Download de historico de jornadas via BLE (bulk transfer com sequence numbers)

### Funcionalidades Avancadas

- **ADV-01**: Screen dimming automatico por inatividade com wake on touch
- **ADV-02**: BLE-triggered audio test para verificacao remota do speaker
- **ADV-03**: Alertas de RPM/velocidade com audio quando limiar excedido (requer fonte de dados externa)
- **ADV-04**: Integracao com adaptador OBD externo para dados reais de RPM/velocidade via BLE
- **ADV-05**: Secure Boot v2 para protecao contra firmware nao autorizado

### Robustez

- **ROB-01**: Monitoramento periodico de heap/stack com log serial (heap_caps_get_info, uxTaskGetStackHighWaterMark)
- **ROB-02**: Stress testing: audio + rapid button presses + BLE simultaneous
- **ROB-03**: Testes end-to-end: ignicao -> audio -> status update -> BLE notification

## Out of Scope

Explicitamente excluido. Documentado para prevenir scope creep.

| Feature | Razao |
|---------|-------|
| Wi-Fi connectivity | BLE suficiente; Wi-Fi consome ~40KB RAM extra e adiciona complexidade |
| GPS/GNSS | Sem hardware GPS; rastreadores dedicados ja instalados nos veiculos |
| Cloud/server direto do dispositivo | Dispositivo offline-first; celular faz ponte BLE-to-cloud |
| App mobile (Android/iOS) | Desenvolvido separadamente apos firmware; protocolo BLE sera a interface |
| Teclado fisico/botoes externos | Somente touchscreen |
| LVGL Tabview para navegacao | Consome 40-60px de tela; screen manager com stack e melhor |
| LVGL Tileview para navegacao | Conflita com swipe existente entre numpad/jornada |
| LVGL Fragment module | API C-struct incompativel com arquitetura C++ existente |
| Protocolo BLE custom (non-GATT) | Quebra compatibilidade com frameworks BLE nativos iOS/Android |
| Streaming de audio via BLE | Bandwidth BLE insuficiente (~30-50KB/s); audio local em LittleFS |
| Implementacao OBD-II completa no dispositivo | Responsabilidade do adaptador externo; dispositivo recebe valores processados |
| UI multi-idioma | Portugues-BR e o mercado alvo; internacionalizacao no app primeiro |
| File manager na tela | UX ruim em 3.5" com luvas; operacoes de arquivo pelo app via BLE |

## Traceability

| Requirement | Phase | Status |
|-------------|-------|--------|
| INFRA-01 | Phase 1 | Pending |
| INFRA-02 | Phase 2 | Pending |
| INFRA-03 | Phase 2 | Pending |
| INFRA-04 | Phase 1 | Pending |
| INFRA-05 | Phase 1 | Pending |
| SCR-01 | Phase 1 | Pending |
| SCR-02 | Phase 1 | Pending |
| SCR-03 | Phase 1 | Pending |
| SCR-04 | Phase 1 | Pending |
| SCR-05 | Phase 1 | Pending |
| SCR-06 | Phase 1 | Pending |
| SCR-07 | Phase 1 | Pending |
| SCR-08 | Phase 1 | Pending |
| NVS-01 | Phase 1 | Pending |
| NVS-02 | Phase 1 | Pending |
| NVS-03 | Phase 1 | Pending |
| NVS-04 | Phase 3 | Pending |
| BLE-01 | Phase 2 | Pending |
| BLE-02 | Phase 2 | Pending |
| BLE-03 | Phase 2 | Pending |
| BLE-04 | Phase 2 | Pending |
| BLE-05 | Phase 2 | Pending |
| BLE-06 | Phase 2 | Pending |
| BLE-07 | Phase 2 | Pending |
| BLE-08 | Phase 2 | Pending |
| GATT-01 | Phase 2 | Pending |
| GATT-02 | Phase 2 | Pending |
| GATT-03 | Phase 2 | Pending |
| GATT-04 | Phase 2 | Pending |
| GATT-05 | Phase 3 | Pending |
| GATT-06 | Phase 3 | Pending |
| GATT-07 | Phase 3 | Pending |
| GATT-08 | Phase 2 | Pending |
| GATT-09 | Phase 2 | Pending |
| CFG-01 | Phase 3 | Pending |
| CFG-02 | Phase 3 | Pending |
| CFG-03 | Phase 3 | Pending |
| CFG-04 | Phase 3 | Pending |
| CFG-05 | Phase 3 | Pending |
| RPM-01 | Phase 5 | Pending |
| RPM-02 | Phase 5 | Pending |
| RPM-03 | Phase 5 | Pending |
| RPM-04 | Phase 5 | Pending |
| OTA-01 | Phase 4 | Pending |
| OTA-02 | Phase 4 | Pending |
| OTA-03 | Phase 4 | Pending |
| OTA-04 | Phase 4 | Pending |
| OTA-05 | Phase 4 | Pending |
| OTA-06 | Phase 4 | Pending |
| OTA-07 | Phase 4 | Pending |
| OTA-08 | Phase 4 | Pending |
| DOC-01 | Phase 5 | Pending |
| DOC-02 | Phase 5 | Pending |
| DOC-03 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 54 total (9 categories: INFRA, SCR, NVS, BLE, GATT, CFG, RPM, OTA, DOC)
- Mapped to phases: 54
- Unmapped: 0

---
*Requirements defined: 2026-02-09*
*Last updated: 2026-02-09 after roadmap creation*
