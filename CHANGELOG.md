# Changelog

Todas as mudancas notaveis neste projeto serao documentadas neste arquivo.

O formato e baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/),
e este projeto adere ao [Versionamento Semantico](https://semver.org/lang/pt-BR/).

---

## [1.1.0] - 2026-01-13

### Adicionado

#### Nova Arquitetura Modular
- Sistema de configuracao centralizado (`include/config/app_config.h`)
- Interfaces abstratas para desacoplamento de modulos:
  - `i_audio.h` - Interface de player de audio
  - `i_ignicao.h` - Interface de monitoramento de ignicao
  - `i_jornada.h` - Interface de gerenciamento de jornada
  - `i_screen.h` - Interface de gerenciamento de telas
- Servicos refatorados com injecao de dependencias:
  - `IgnicaoService` - Servico de ignicao com estatisticas
  - `JornadaService` - Servico de jornada thread-safe
- Utilitarios centralizados:
  - `TimeFormatter` - Formatacao de tempo thread-safe
  - `debug_utils` - Macros de log e informacoes de heap
- Componentes de UI reutilizaveis:
  - `Theme` - Gerenciamento centralizado de tema
  - `StatusBar` - Widget de barra de status modular
- Modulo de inicializacao (`app_init`) para startup organizado

#### Nova Estrutura de Diretorios
```
src/
  core/        - Inicializacao e orquestracao
  hal/         - Hardware Abstraction Layer
  services/    - Logica de negocios
  ui/          - Interface grafica
  utils/       - Funcoes utilitarias
include/
  config/      - Configuracoes centralizadas
  interfaces/  - Interfaces abstratas
  core/        - Headers do core
  services/    - Headers dos servicos
  ui/          - Headers da UI
  utils/       - Headers dos utilitarios
```

### Corrigido
- Thread-safety em `formatTime()` - buffer estatico substituido por classe thread-safe
- Potencial race condition em `getMotorista()` - adicionado mutex
- Callback `onJornadaStateChange()` indefinido - implementacao adicionada

### Modificado
- Valores hardcoded movidos para `app_config.h`
- CMakeLists.txt atualizado com novos diretorios de include
- Separacao clara entre HAL, Services e UI
- Padronizacao de nomes e convencoes de codigo

### Tecnico
- Preparacao para crescimento significativo de codigo
- Facilita testes unitarios com interfaces abstratas
- Melhora manutencao com modulos independentes
- Reduz acoplamento entre componentes

---

## [1.0.0] - 2026-01-13

### Adicionado

#### Sistema de Jornada
- Gerenciamento de ate 3 motoristas simultaneos
- 7 estados de jornada: Inativo, Jornada, Manobra, Refeicao, Espera, Descarga, Abastecimento
- Tracking automatico de tempo por estado
- Estatisticas em tempo real por motorista

#### Interface Grafica (LVGL)
- Display touchscreen 3.5" 320x480
- Teclado de jornada com 12 botoes interativos
- Teclado numerico com timeout automatico
- Navegacao por swipe entre telas
- Barra de status com tempo de ignicao e jornada
- Sistema de popups (Info, Warning, Error, Question, Success)
- Tela de splash com logo PNG

#### Monitoramento de Ignicao
- Deteccao automatica de estado ON/OFF (GPIO18)
- Debounce inteligente configuravel (1-10 segundos)
- Tempos independentes para ON e OFF
- Callback para mudancas de estado
- Contador de tempo de ignicao na barra de status

#### Sistema de Audio
- Reprodutor MP3 com decoder minimp3
- Saida I2S com taxa de amostragem adaptativa
- Controle de volume (0-21 niveis)
- Queue de ate 4 requisicoes simultaneas
- Task dedicada no Core 1
- 11 arquivos de audio para feedback:
  - Eventos de ignicao
  - Cliques de interface
  - Alertas e confirmacoes
  - Solicutacao de identificacao

#### Hardware Support
- ESP32-S3 com dual-core 240MHz
- Display QSPI com controlador AXS15231B
- Touch capacitivo I2C
- Filesystem LittleFS com 1MB
- Suporte a PSRAM

#### Arquitetura
- Framework ESP-IDF puro (sem Arduino)
- Multi-core: LVGL no Core 0, Audio no Core 1
- Thread-safe com mutex em todos os modulos
- Sistema de retry automatico para criacao de botoes
- Board Support Package modular

### Tecnico
- Particionamento: 3MB app + 1MB filesystem
- Flash mode QIO 80MHz
- Upload via CDC/Serial 921600 baud
- Compativel com PlatformIO

---

## Tipos de Mudanca

- `Adicionado` - Novas funcionalidades
- `Modificado` - Alteracoes em funcionalidades existentes
- `Obsoleto` - Funcionalidades que serao removidas em breve
- `Removido` - Funcionalidades removidas
- `Corrigido` - Correcoes de bugs
- `Seguranca` - Correcoes de vulnerabilidades

---

Copyright (c) 2024-2026 Getscale Sistemas Embarcados
Desenvolvido por Mario Stanski Jr
