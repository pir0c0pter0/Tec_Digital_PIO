# Arquitetura do Sistema - Teclado de Jornada Digital

## Sumario

- [Visao Geral](#visao-geral)
- [Estrutura de Diretorios](#estrutura-de-diretorios)
- [Camadas da Arquitetura](#camadas-da-arquitetura)
- [Configuracao Centralizada](#configuracao-centralizada)
- [Interfaces Abstratas](#interfaces-abstratas)
- [Servicos](#servicos)
- [Interface Grafica (UI)](#interface-grafica-ui)
- [Utilitarios](#utilitarios)
- [Fluxo de Inicializacao](#fluxo-de-inicializacao)
- [Modelo de Concorrencia](#modelo-de-concorrencia)
- [Guia para Novos Modulos](#guia-para-novos-modulos)

---

## Visao Geral

O Teclado de Jornada Digital utiliza uma arquitetura modular em camadas, projetada para:

- **Escalabilidade**: Facilitar adicao de novos recursos sem impactar codigo existente
- **Testabilidade**: Interfaces abstratas permitem mocks e testes unitarios
- **Manutencao**: Separacao clara de responsabilidades
- **Reutilizacao**: Componentes independentes e reutilizaveis

### Stack Tecnologico

| Componente | Tecnologia |
|------------|------------|
| MCU | ESP32-S3 (Dual-core 240MHz) |
| Framework | ESP-IDF 5.3.1 |
| RTOS | FreeRTOS |
| GUI | LVGL 8.4.0 |
| Build | PlatformIO + CMake |
| Filesystem | LittleFS |

---

## Estrutura de Diretorios

```
sketch_oct8a_pio/
├── include/                    # Headers publicos
│   ├── config/                 # Configuracoes centralizadas
│   │   └── app_config.h        # Todas as constantes do sistema
│   │
│   ├── interfaces/             # Interfaces abstratas (contratos)
│   │   ├── i_audio.h           # Interface de audio
│   │   ├── i_ignicao.h         # Interface de ignicao
│   │   ├── i_jornada.h         # Interface de jornada
│   │   └── i_screen.h          # Interface de telas
│   │
│   ├── core/                   # Headers do nucleo
│   │   └── app_init.h          # Inicializacao da aplicacao
│   │
│   ├── services/               # Headers dos servicos
│   │   ├── ignicao/
│   │   │   └── ignicao_service.h
│   │   └── jornada/
│   │       └── jornada_service.h
│   │
│   ├── ui/                     # Headers da UI
│   │   ├── common/
│   │   │   └── theme.h         # Sistema de temas
│   │   └── widgets/
│   │       └── status_bar.h    # Barra de status
│   │
│   ├── utils/                  # Headers utilitarios
│   │   ├── time_utils.h        # Formatacao de tempo
│   │   └── debug_utils.h       # Macros de debug
│   │
│   └── *.h                     # Headers legados (compatibilidade)
│
├── src/                        # Codigo fonte
│   ├── core/                   # Nucleo da aplicacao
│   │   └── app_init.cpp        # Inicializacao (filesystem, versao)
│   │
│   ├── services/               # Servicos de negocio
│   │   ├── ignicao/
│   │   │   └── ignicao_service.cpp
│   │   └── jornada/
│   │       └── jornada_service.cpp
│   │
│   ├── ui/                     # Interface grafica
│   │   ├── common/
│   │   │   └── theme.cpp       # Definicoes de tema
│   │   └── widgets/
│   │       └── status_bar.cpp  # Widget de status
│   │
│   ├── utils/                  # Utilitarios
│   │   ├── time_utils.cpp      # TimeFormatter thread-safe
│   │   └── debug_utils.cpp     # Funcoes de debug
│   │
│   ├── main.cpp                # Ponto de entrada
│   └── *.cpp                   # Modulos legados
│
├── lib/                        # Bibliotecas externas
│   ├── lvgl/                   # LVGL 8.4.0
│   └── ESP32-audioI2S/         # Driver de audio
│
├── data/                       # Arquivos para LittleFS
│   ├── *.mp3                   # Arquivos de audio
│   └── *.png                   # Imagens
│
└── docs/                       # Documentacao
    └── ARCHITECTURE.md         # Este arquivo
```

---

## Camadas da Arquitetura

```
┌─────────────────────────────────────────────────────────────┐
│                      APLICACAO (main.cpp)                   │
│                  Orquestracao e ciclo principal             │
├─────────────────────────────────────────────────────────────┤
│                         SERVICOS                            │
│        JornadaService  │  IgnicaoService  │  AudioService   │
├─────────────────────────────────────────────────────────────┤
│                      INTERFACES                             │
│      IJornadaService  │  IIgnicaoService  │  IAudioPlayer   │
├─────────────────────────────────────────────────────────────┤
│                     UI COMPONENTS                           │
│          Theme  │  StatusBar  │  Screens  │  Widgets        │
├─────────────────────────────────────────────────────────────┤
│                       UTILITARIOS                           │
│            TimeFormatter  │  DebugUtils  │  Helpers         │
├─────────────────────────────────────────────────────────────┤
│                    HAL / DRIVERS                            │
│       Display  │  Touch  │  GPIO  │  I2S  │  LittleFS       │
├─────────────────────────────────────────────────────────────┤
│                      ESP-IDF / FreeRTOS                     │
└─────────────────────────────────────────────────────────────┘
```

### Principios

1. **Dependencia unidirecional**: Camadas superiores dependem das inferiores
2. **Inversao de dependencia**: Servicos dependem de interfaces, nao implementacoes
3. **Separacao de responsabilidades**: Cada modulo tem uma unica responsabilidade

---

## Configuracao Centralizada

### Arquivo: `include/config/app_config.h`

Todas as constantes do sistema estao centralizadas em um unico arquivo:

```c
// ============================================================================
// INFORMACOES DA APLICACAO
// ============================================================================
#define APP_NAME                "Teclado de Jornada Digital"
#define APP_VERSION_STRING      "1.1.0"
#define APP_AUTHOR              "Mario Stanski Jr"
#define APP_COMPANY             "Getscale Sistemas Embarcados"

// ============================================================================
// CONFIGURACOES DE DISPLAY
// ============================================================================
#define DISPLAY_WIDTH           480
#define DISPLAY_HEIGHT          320
#define STATUS_BAR_HEIGHT       40
#define GRID_AREA_HEIGHT        (DISPLAY_HEIGHT - STATUS_BAR_HEIGHT)

// ============================================================================
// CONFIGURACOES DE GRID
// ============================================================================
#define GRID_COLS               4
#define GRID_ROWS               3
#define MAX_MOTORISTAS          3

// ============================================================================
// CONFIGURACOES DE GPIO
// ============================================================================
#define PIN_IGNICAO             GPIO_NUM_18
#define PIN_I2S_BCLK            GPIO_NUM_16
#define PIN_I2S_LRC             GPIO_NUM_7
#define PIN_I2S_DOUT            GPIO_NUM_6

// ============================================================================
// CONFIGURACOES DE TASKS
// ============================================================================
#define SYSTEM_TASK_STACK_SIZE  8192
#define SYSTEM_TASK_PRIORITY    5
#define SYSTEM_TASK_CORE        0
#define AUDIO_TASK_CORE         1

// ============================================================================
// CORES DO TEMA
// ============================================================================
#define COLOR_PRIMARY           0x2196F3
#define COLOR_SUCCESS           0x4CAF50
#define COLOR_WARNING           0xFFC107
#define COLOR_ERROR             0xF44336
```

### Beneficios

- **Facil manutencao**: Alterar valores em um unico local
- **Consistencia**: Todos os modulos usam os mesmos valores
- **Documentacao**: Constantes agrupadas por categoria

---

## Interfaces Abstratas

As interfaces definem contratos que os servicos devem implementar, permitindo:

- Substituicao de implementacoes
- Testes com mocks
- Baixo acoplamento

### IJornadaService (`include/interfaces/i_jornada.h`)

```cpp
// Estados possiveis de jornada
enum class EstadoJornada {
    INATIVO = 0,
    JORNADA,
    MANOBRA,
    REFEICAO,
    ESPERA,
    DESCARGA,
    ABASTECIMENTO
};

// Dados de um motorista
struct DadosMotorista {
    int id;
    char nome[MAX_NOME_MOTORISTA];
    bool ativo;
    EstadoJornada estadoAtual;
    uint32_t inicioEstadoAtual;
    uint32_t tempoAcumulado[7];
};

// Interface do servico
class IJornadaService {
public:
    virtual ~IJornadaService() = default;

    virtual void init() = 0;
    virtual bool addMotorista(int id, const char* nome) = 0;
    virtual void removeMotorista(int id) = 0;
    virtual bool getMotorista(int id, DadosMotorista* dados) const = 0;
    virtual int getNumMotoristasAtivos() const = 0;
    virtual bool iniciarEstado(int id, EstadoJornada estado) = 0;
    virtual bool finalizarEstado(int id) = 0;
    virtual bool temJornadaAtiva() const = 0;
    virtual uint32_t getTempoEstadoAtual(int id) const = 0;
    virtual void setCallback(JornadaCallback callback) = 0;
};
```

### IIgnicaoService (`include/interfaces/i_ignicao.h`)

```cpp
// Callback de mudanca de estado
typedef void (*IgnicaoCallback)(bool ligada);

// Estatisticas de ignicao
struct IgnicaoStats {
    uint32_t totalLigacoes;
    uint32_t tempoTotalLigado;
    uint32_t ultimaLigacao;
};

// Interface do servico
class IIgnicaoService {
public:
    virtual ~IIgnicaoService() = default;

    virtual bool init(float debounceOn, float debounceOff, bool pullup) = 0;
    virtual bool getStatus() const = 0;
    virtual uint32_t getTempoLigado() const = 0;
    virtual void getStats(IgnicaoStats* stats) const = 0;
    virtual void setCallback(IgnicaoCallback callback) = 0;
};
```

### IAudioPlayer (`include/interfaces/i_audio.h`)

```cpp
class IAudioPlayer {
public:
    virtual ~IAudioPlayer() = default;

    virtual bool init() = 0;
    virtual bool play(const char* filename) = 0;
    virtual void stop() = 0;
    virtual void setVolume(int level) = 0;
    virtual int getVolume() const = 0;
    virtual bool isPlaying() const = 0;
};
```

---

## Servicos

Os servicos implementam a logica de negocio e seguem o padrao Singleton.

### JornadaService (`src/services/jornada/jornada_service.cpp`)

```cpp
class JornadaService : public IJornadaService {
public:
    // Singleton thread-safe
    static JornadaService* getInstance();
    static void destroyInstance();

    // Implementacao da interface
    void init() override;
    bool addMotorista(int id, const char* nome) override;
    // ... demais metodos

private:
    JornadaService();
    ~JornadaService();

    // Dados protegidos por mutex
    DadosMotorista motoristas[MAX_MOTORISTAS];
    mutable SemaphoreHandle_t mutex;

    // Metodos auxiliares
    int findMotoristaIndex(int id) const;
    void atualizarTempoAcumulado(int idx);
};
```

### Caracteristicas dos Servicos

| Caracteristica | Descricao |
|----------------|-----------|
| **Singleton** | Uma unica instancia global |
| **Thread-safe** | Mutex protege dados compartilhados |
| **Lazy init** | Criado na primeira chamada |
| **Callback** | Notifica mudancas de estado |

---

## Interface Grafica (UI)

### Sistema de Temas (`src/ui/common/theme.cpp`)

```cpp
class Theme {
public:
    static void init();

    // Cores
    static lv_color_t getPrimary();
    static lv_color_t getSuccess();
    static lv_color_t getWarning();
    static lv_color_t getError();
    static lv_color_t getBackground();
    static lv_color_t getText();

    // Estilos pre-definidos
    static lv_style_t* getButtonStyle();
    static lv_style_t* getStatusBarStyle();

    // Aplicar tema global
    static void applyToScreen(lv_obj_t* screen);
};
```

### Barra de Status (`src/ui/widgets/status_bar.cpp`)

Widget reutilizavel para exibir informacoes no topo da tela:

```cpp
class StatusBar {
public:
    static StatusBar* create(lv_obj_t* parent);

    void setIgnicaoStatus(bool ligada);
    void setTempoIgnicao(uint32_t ms);
    void setTempoJornada(uint32_t ms);
    void setMessage(const char* msg);
    void update();

private:
    lv_obj_t* container;
    lv_obj_t* lblIgnicao;
    lv_obj_t* lblTempoIgnicao;
    lv_obj_t* lblTempoJornada;
    lv_obj_t* lblMessage;
};
```

---

## Utilitarios

### TimeFormatter (`src/utils/time_utils.cpp`)

Formatacao de tempo **thread-safe** (corrige bug da versao anterior):

```cpp
class TimeFormatter {
public:
    // Formata milissegundos para "HH:MM:SS"
    // Thread-safe: cada chamada usa buffer proprio
    static void format(uint32_t ms, char* buffer, size_t size);

    // Formata para "Xh Ym" (formato curto)
    static void formatShort(uint32_t ms, char* buffer, size_t size);

    // Retorna apenas horas
    static uint32_t getHours(uint32_t ms);

    // Retorna apenas minutos
    static uint32_t getMinutes(uint32_t ms);
};

// Funcoes C para compatibilidade
extern "C" {
    uint32_t time_millis(void);        // Wrapper para esp_timer
    void time_delay_ms(uint32_t ms);   // Wrapper para vTaskDelay
}
```

### DebugUtils (`src/utils/debug_utils.cpp`)

Macros de log e debug:

```cpp
// Macros de log simplificadas
#define LOG_TAG(tag) static const char* TAG = tag

#define LOG_I(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#define LOG_W(tag, fmt, ...) ESP_LOGW(tag, fmt, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#define LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)

// Funcoes de debug
void debug_print_heap_info(void);      // Mostra uso de memoria
void debug_print_task_info(void);      // Mostra tasks ativas
void debug_assert(bool condition, const char* msg);
```

---

## Fluxo de Inicializacao

```
app_main()
    │
    ├── app_print_version()           // Exibe versao no log
    │
    ├── app_init_filesystem()         // Monta LittleFS
    │   └── Lista arquivos disponiveis
    │
    ├── bsp_display_start()           // Inicializa display QSPI
    │   └── Configura rotacao 90°
    │
    ├── lvgl_fs_init('A')             // Driver FS para LVGL
    │
    ├── lv_png_init()                 // Decoder PNG
    │
    ├── createSplashScreen()          // Exibe splash
    │
    └── xTaskCreatePinnedToCore()     // Cria task principal
            │
            └── system_task()
                    │
                    ├── Aguarda splash terminar
                    │
                    ├── initSimpleAudio()     // Audio no Core 1
                    │
                    ├── buttonManagerInit()   // Interface de botoes
                    │
                    ├── numpadInit()          // Teclado numerico
                    │
                    ├── initScreenManager()   // Gerenciador de telas
                    │
                    ├── initIgnicaoControl()  // Monitoramento GPIO
                    │
                    └── Loop principal
                        ├── Atualiza status bar (1Hz)
                        └── lv_timer_handler() (200Hz)
```

---

## Modelo de Concorrencia

### Distribuicao de Cores

| Core | Responsabilidade | Tasks |
|------|------------------|-------|
| **Core 0** | Interface grafica | system_task, LVGL timer |
| **Core 1** | Audio | audio_task |

### Sincronizacao

```cpp
// Mutex para dados compartilhados
SemaphoreHandle_t mutex = xSemaphoreCreateMutex();

// Padrao de acesso seguro
bool getData(Data* out) {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(out, &internalData, sizeof(Data));
        xSemaphoreGive(mutex);
        return true;
    }
    return false;
}
```

### Queue de Audio

```cpp
// Fila para requisicoes de audio (evita bloqueio da UI)
QueueHandle_t audioQueue = xQueueCreate(4, sizeof(AudioRequest));

// Task de audio consome a fila
void audio_task(void* arg) {
    AudioRequest req;
    while (1) {
        if (xQueueReceive(audioQueue, &req, portMAX_DELAY)) {
            playFile(req.filename);
        }
    }
}
```

---

## Guia para Novos Modulos

### 1. Criar Interface (se necessario)

```cpp
// include/interfaces/i_novo_servico.h
#ifndef I_NOVO_SERVICO_H
#define I_NOVO_SERVICO_H

class INovoServico {
public:
    virtual ~INovoServico() = default;
    virtual void init() = 0;
    virtual void doSomething() = 0;
};

#endif
```

### 2. Implementar Servico

```cpp
// include/services/novo/novo_service.h
#ifndef NOVO_SERVICE_H
#define NOVO_SERVICE_H

#include "config/app_config.h"
#include "interfaces/i_novo_servico.h"

class NovoService : public INovoServico {
public:
    static NovoService* getInstance();
    static void destroyInstance();

    void init() override;
    void doSomething() override;

private:
    NovoService();
    ~NovoService();

    static NovoService* instance;
    SemaphoreHandle_t mutex;
};

#endif
```

```cpp
// src/services/novo/novo_service.cpp
#include "services/novo/novo_service.h"
#include "utils/debug_utils.h"

LOG_TAG("NOVO_SERVICE");

NovoService* NovoService::instance = nullptr;

NovoService* NovoService::getInstance() {
    if (!instance) {
        instance = new NovoService();
    }
    return instance;
}

NovoService::NovoService() {
    mutex = xSemaphoreCreateMutex();
    LOG_I(TAG, "Servico criado");
}

void NovoService::init() {
    LOG_I(TAG, "Inicializando...");
}

void NovoService::doSomething() {
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Logica aqui
        xSemaphoreGive(mutex);
    }
}
```

### 3. Atualizar CMakeLists.txt

Adicionar novo diretorio de include se necessario:

```cmake
set(INCLUDE_DIRS
    # ... existentes ...
    ${CMAKE_SOURCE_DIR}/include/services/novo
)
```

### 4. Usar no main.cpp

```cpp
#include "services/novo/novo_service.h"

void app_main() {
    // ...
    NovoService::getInstance()->init();
    // ...
}
```

---

## Historico de Versoes da Arquitetura

| Versao | Data | Descricao |
|--------|------|-----------|
| 1.0.0 | 2026-01-13 | Arquitetura monolitica inicial |
| 1.1.0 | 2026-01-13 | Refatoracao modular completa |

---

**Copyright (c) 2024-2026 Getscale Sistemas Embarcados**
**Desenvolvido por Mario Stanski Jr**
