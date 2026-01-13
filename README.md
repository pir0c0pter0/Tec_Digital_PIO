# Teclado de Jornada Digital

Sistema embarcado profissional para monitoramento de jornada de motoristas, desenvolvido com ESP32-S3 e interface touchscreen LVGL.

---

## Sobre o Projeto

O **Teclado de Jornada Digital** e um sistema completo para gerenciamento e monitoramento de jornadas de trabalho de motoristas profissionais. Desenvolvido para atender regulamentacoes de controle de jornada, o sistema oferece interface intuitiva touchscreen, feedback sonoro e monitoramento em tempo real.

### Principais Funcionalidades

- **Gerenciamento de Motoristas**: Suporte para ate 3 motoristas simultaneos
- **Estados de Jornada**: 7 estados configurados (Jornada, Manobra, Refeicao, Espera, Descarga, Abastecimento, Inativo)
- **Monitoramento de Ignicao**: Deteccao automatica com debounce inteligente
- **Interface Touchscreen**: Display colorido de 3.5" com LVGL
- **Feedback Sonoro**: Alertas e confirmacoes em MP3
- **Navegacao Intuitiva**: Swipe para alternar entre telas

---

## Especificacoes Tecnicas

### Hardware

| Componente | Especificacao |
|------------|---------------|
| **Microcontrolador** | ESP32-S3 (Dual-core 240MHz) |
| **Display** | LCD QSPI 3.5" 320x480 (JC3248W535EN) |
| **Controlador LCD** | AXS15231B |
| **Touch** | Capacitivo I2C |
| **Audio** | I2S com decoder minimp3 |
| **Flash** | 16MB (3MB App + 1MB LittleFS) |
| **PSRAM** | Disponivel |

### Pinagem Principal

```
Display QSPI:
  CS: GPIO45  |  SCK: GPIO47  |  D0: GPIO21
  D1: GPIO48  |  D2: GPIO40   |  D3: GPIO39
  TE: GPIO38  |  BLK: GPIO1

Touch I2C:
  SCL: GPIO8  |  SDA: GPIO4   |  INT: GPIO3

Audio I2S:
  BCK: GPIO42 |  LRCK: GPIO2  |  DO: GPIO41

Monitoramento:
  Ignicao: GPIO18  |  Bateria ADC: GPIO5
```

---

## Arquitetura do Software

### Estrutura de Diretorios

```
Teclado-de-Jornada-Digital/
├── src/                    # Codigo fonte principal
│   ├── main.cpp           # Ponto de entrada
│   ├── button_manager.cpp # Gerenciador de botoes
│   ├── ignicao_control.cpp# Controle de ignicao
│   ├── jornada_manager.cpp# Gerenciador de jornada
│   ├── jornada_keyboard.cpp# Teclado de jornada
│   ├── numpad_example.cpp # Teclado numerico
│   ├── simple_audio_manager.cpp # Audio MP3
│   ├── simple_splash.cpp  # Tela de splash
│   ├── esp_bsp.c          # Board Support Package
│   ├── esp_lcd_*.c        # Drivers LCD
│   └── lv_port.c          # Adaptacao LVGL
├── include/                # Headers
├── data/                   # Assets (audio, imagens)
├── lib/                    # Bibliotecas (LVGL)
├── managed_components/     # Componentes (LittleFS)
├── platformio.ini          # Configuracao PlatformIO
└── partitions.csv          # Particoes de flash
```

### Diagrama de Modulos

```
┌─────────────────────────────────────────────────────┐
│                    main.cpp                         │
│              (Inicializacao e Loop)                 │
└───────────────────────┬─────────────────────────────┘
                        │
    ┌───────────────────┼───────────────────┐
    │                   │                   │
    ▼                   ▼                   ▼
┌────────────┐   ┌────────────┐   ┌────────────────┐
│ Jornada    │   │ Ignicao    │   │ Audio          │
│ Manager    │   │ Control    │   │ Manager        │
└────────────┘   └────────────┘   └────────────────┘
    │                   │                   │
    │                   │                   │
    └───────────────────┼───────────────────┘
                        │
                        ▼
              ┌────────────────┐
              │ Button Manager │
              │    (LVGL UI)   │
              └────────────────┘
                        │
              ┌─────────┴─────────┐
              │                   │
              ▼                   ▼
       ┌────────────┐     ┌────────────┐
       │  Jornada   │     │   Numpad   │
       │  Keyboard  │     │  Keyboard  │
       └────────────┘     └────────────┘
```

---

## Estados de Jornada

O sistema suporta 7 estados de trabalho para cada motorista:

| Estado | Descricao | Icone |
|--------|-----------|-------|
| **INATIVO** | Motorista fora de servico | - |
| **JORNADA** | Em conducao ativa | Volante |
| **MANOBRA** | Realizando manobras | Maos no volante |
| **REFEICAO** | Intervalo para refeicao | Garfo/faca |
| **ESPERA** | Aguardando | Relogio |
| **DESCARGA** | Operacao de descarga | Caixa |
| **ABASTECIMENTO** | Abastecendo veiculo | Bomba |

---

## Compilacao e Upload

### Pre-requisitos

- [PlatformIO](https://platformio.org/) (VS Code Extension ou CLI)
- Python 3.x
- Driver USB para ESP32-S3

### Passos

1. **Clone o repositorio**
```bash
git clone https://github.com/pir0c0pter0/Teclado-de-Jornada-Digital.git
cd Teclado-de-Jornada-Digital
```

2. **Compile o projeto**
```bash
pio run
```

3. **Upload do firmware**
```bash
pio run --target upload
```

4. **Upload do filesystem (assets)**
```bash
pio run --target uploadfs
```

5. **Monitor serial**
```bash
pio device monitor
```

---

## Configuracao

### Particoes de Flash

```
nvs      : 20KB  - Dados persistentes
phy_init : 4KB   - Calibracao RF
app0     : 3MB   - Aplicacao principal
spiffs   : 1MB   - Filesystem (LittleFS)
```

### Configuracoes de Compilacao

```ini
; platformio.ini - Principais opcoes
board_build.flash_size = 16MB
board_build.flash_mode = qio
board_build.f_flash = 80000000L
board_build.f_cpu = 240000000L
```

---

## Assets de Audio

O sistema inclui os seguintes arquivos de audio para feedback:

| Arquivo | Funcao |
|---------|--------|
| `ign_on_jornada_manobra.mp3` | Ignicao ligada |
| `ign_off.mp3` | Ignicao desligada |
| `click.mp3` | Clique de botao |
| `ok_click.mp3` | Confirmacao |
| `nok_click.mp3` | Negacao |
| `nok_user.mp3` | Erro de usuario |
| `identificacao_ok.mp3` | Identificacao aceita |
| `digite_senha.mp3` | Solicitar senha |
| `aproxime_rfid.mp3` | Solicitar RFID |
| `alerta_max_rpm.mp3` | Alerta de RPM |
| `alerta_max_vel.mp3` | Alerta de velocidade |

---

## Versionamento

Este projeto segue o [Versionamento Semantico](https://semver.org/lang/pt-BR/):

- **MAJOR**: Mudancas incompativeis na API
- **MINOR**: Novas funcionalidades compativeis
- **PATCH**: Correcoes de bugs compativeis

Versao atual: Consulte o arquivo `VERSION`

### Historico de Versoes

- **v1.0.0** - Release inicial
  - Sistema de jornada com 7 estados
  - Suporte a 3 motoristas
  - Interface touchscreen LVGL
  - Monitoramento de ignicao
  - Feedback sonoro MP3
  - Tela de splash com logo

---

## Licenca

**TODOS OS DIREITOS RESERVADOS**

Copyright (c) 2024-2026 **Getscale Sistemas Embarcados**

Este software e propriedade exclusiva da Getscale Sistemas Embarcados.
Nenhuma parte deste software pode ser copiada, modificada, distribuida
ou utilizada sem autorizacao previa por escrito.

---

## Desenvolvedor

**Mario Stanski Jr**

Desenvolvido para Getscale Sistemas Embarcados

---

## Contato

Para informacoes comerciais ou suporte tecnico:

**Getscale Sistemas Embarcados**

---

## Agradecimentos

- [Espressif Systems](https://www.espressif.com/) - ESP-IDF Framework
- [LVGL](https://lvgl.io/) - Light and Versatile Graphics Library
- [PlatformIO](https://platformio.org/) - Ambiente de desenvolvimento
- [minimp3](https://github.com/lieff/minimp3) - Decoder de audio MP3
