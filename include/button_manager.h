/**
 * ============================================================================
 * GERENCIADOR DE BOTÕES - HEADER COM SISTEMA ROBUSTO DE CRIAÇÃO
 * ============================================================================
 */

#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <lvgl.h>
#include <vector>
#include <functional>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Incluir configuracao centralizada (se disponivel)
#if __has_include("config/app_config.h")
    #include "config/app_config.h"
#endif

// ============================================================================
// CONFIGURAÇÕES DE TELA (compatibilidade com codigo legado)
// ============================================================================

#ifndef DISPLAY_WIDTH
    #define SCREEN_WIDTH 480
#else
    #define SCREEN_WIDTH DISPLAY_WIDTH
#endif

#ifndef DISPLAY_HEIGHT
    #define SCREEN_HEIGHT 320
#else
    #define SCREEN_HEIGHT DISPLAY_HEIGHT
#endif

#ifndef STATUS_BAR_HEIGHT
    #define STATUS_BAR_HEIGHT 40
#endif

#ifndef GRID_AREA_HEIGHT
    #define GRID_AREA_HEIGHT (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)
#endif

// ============================================================================
// CONFIGURAÇÕES DA GRADE (compatibilidade com codigo legado)
// ============================================================================

#ifndef GRID_COLS
    #define GRID_COLS 4
#endif

#ifndef GRID_ROWS
    #define GRID_ROWS 3
#endif

#define BUTTON_WIDTH 110
#define BUTTON_HEIGHT 85
#define BUTTON_MARGIN 5
#define GRID_PADDING 10

// ============================================================================
// ENUMS
// ============================================================================

enum ButtonIcon {
    ICON_NONE = 0,
    ICON_STEERING,
    ICON_CLOCK,
    ICON_FOOD,
    ICON_FUEL,
    ICON_TRUCK,
    ICON_BOX,
    ICON_WRENCH,
    ICON_USER,
    ICON_MAP,
    ICON_SETTINGS,
    ICON_POWER,
    ICON_PAUSE,
    ICON_PLAY,
    ICON_STOP,
    ICON_CHECK,
    ICON_CANCEL,
    ICON_WARNING,
    ICON_INFO,
    ICON_HOME,
    ICON_CHART
};

enum PopupType {
    POPUP_INFO,
    POPUP_WARNING,
    POPUP_ERROR,
    POPUP_QUESTION,
    POPUP_SUCCESS
};

enum PopupResult {
    POPUP_RESULT_NONE,
    POPUP_RESULT_OK,
    POPUP_RESULT_CANCEL
};

// ============================================================================
// ESTRUTURAS
// ============================================================================

struct GridButton {
    int id;
    int gridX, gridY;
    int width, height;
    std::string label;
    ButtonIcon icon;
    lv_color_t color;
    lv_obj_t* obj;
    std::function<void(int)> callback;
    bool enabled;
};

struct BtnStatusBarData {
    bool ignicaoOn;
    unsigned long tempoIgnicao;
    unsigned long tempoJornada;
    const char* mensagemExtra;
};

// ============================================================================
// CLASSE BUTTONMANAGER
// ============================================================================

class ButtonManager {
private:
    static ButtonManager* instance;
    
    // Elementos de UI
    lv_obj_t* screen;
    lv_obj_t* gridContainer;
    lv_obj_t* statusBar;
    lv_obj_t* statusIgnicao;
    lv_obj_t* statusTempoIgnicao;
    lv_obj_t* statusTempoJornada;
    lv_obj_t* statusMensagem;
    lv_timer_t* statusUpdateTimer;
    lv_timer_t* statusTimer;
    
    // Sistema de mensagens
    struct MessageConfig {
        lv_color_t color;
        const lv_font_t* font;
        uint32_t timeoutMs;
        bool hasTimeout;
    } currentMessageConfig;
    
    unsigned long messageExpireTime;
    
    // Sistema de popup
    lv_obj_t* activePopup;
    std::function<void(PopupResult)> popupCallback;
    PopupResult lastPopupResult;
    
    // Sistema de botões
    std::vector<GridButton> buttons;
    bool gridOccupancy[GRID_COLS][GRID_ROWS];
    int nextButtonId;

    // Debounce por instancia (isolamento entre telas)
    unsigned long lastButtonClickTime_;
    int lastButtonClickedId_;
    unsigned long lastPopupClickTime_;
    
    // ==========================================
    // NOVO: Sistema de criação robusta
    // ==========================================
    struct PendingButton {
        int gridX, gridY;
        const char* label;
        ButtonIcon icon;
        const char* image_src;
        lv_color_t color;
        std::function<void(int)> callback;
        int width, height;
        lv_color_t textColor;
        const lv_font_t* textFont;
        int assignedId;
        int retryCount;
    };
    
    std::vector<PendingButton> pendingButtons;
    SemaphoreHandle_t creationMutex;
    lv_timer_t* retryTimer;
    static const int MAX_RETRY_ATTEMPTS = 3;
    static const int RETRY_DELAY_MS = 100;
    
    // Métodos privados para criação robusta
    int addButtonInternal(const PendingButton& btn);
    void processPendingButtons();
    static void retryTimerCallback(lv_timer_t* timer);
    bool verifyButtonCreation(int buttonId);
    
    // Métodos privados existentes
    void createScreen();
    bool isGridPositionFree(int x, int y, int width, int height);
    void markGridPosition(int x, int y, int width, int height, bool occupied);
    bool findFreePosition(int width, int height, int* outX, int* outY);
    bool isPositionValid(int x, int y);
    
    lv_obj_t* createIconForButton(ButtonIcon icon, lv_obj_t* parent,
                                  lv_color_t textColor, const lv_font_t* iconFont);
    const char* getIconText(ButtonIcon icon);
    
    static void buttonEventHandler(lv_event_t* e);
    static void popupButtonHandler(lv_event_t* e);
    static void statusUpdateCallback(lv_timer_t* timer);
    static void statusTimerHandler(lv_timer_t* timer);
    
public:
    ButtonManager();
    ~ButtonManager();

    // Non-copyable (possui objetos LVGL, mutex, timers)
    ButtonManager(const ButtonManager&) = delete;
    ButtonManager& operator=(const ButtonManager&) = delete;

    // Singleton (para compatibilidade com codigo legado)
    static ButtonManager* getInstance();
    static void destroyInstance();
    
    // Inicialização
    void init();

    // Acesso ao screen LVGL interno
    lv_obj_t* getScreen() const { return screen; }
    
    // ==========================================
    // Sistema de botões com criação robusta
    // ==========================================
    int addButton(int gridX, int gridY, const char* label, ButtonIcon icon,
                 const char* image_src,
                 lv_color_t color, std::function<void(int)> callback,
                 int width = 1, int height = 1,
                 lv_color_t textColor = lv_color_hex(0xFFFFFF),
                 const lv_font_t* textFont = &lv_font_montserrat_16);
    
    // NOVO: Criação em lote
    struct ButtonBatchDef {
        int gridX, gridY;
        const char* label;
        ButtonIcon icon;
        const char* image_src;
        lv_color_t color;
        std::function<void(int)> callback;
        int width, height;
        lv_color_t textColor;
        const lv_font_t* textFont;
    };
    
    std::vector<int> addButtonBatch(const std::vector<ButtonBatchDef>& buttonDefs);
    bool waitForAllButtons(const std::vector<int>& buttonIds, int timeoutMs = 1000);
    
    // NOVO: Status de criação
    enum CreationStatus {
        CREATION_SUCCESS,
        CREATION_PENDING,
        CREATION_FAILED
    };
    
    CreationStatus getButtonCreationStatus(int buttonId);
    int getPendingButtonCount() const { return pendingButtons.size(); }
    
    // Gerenciamento de botões existente
    bool removeButton(int buttonId);
    void removeAllButtons();
    bool setButtonEnabled(int buttonId, bool enabled);
    
    bool setButtonLabel(int buttonId, const char* label);
    bool setButtonIcon(int buttonId, ButtonIcon icon);
    bool setButtonColor(int buttonId, lv_color_t color);
    
    GridButton* getButton(int buttonId);
    
    // Barra de status
    void updateStatusBar(const BtnStatusBarData& data);
    void setStatusMessage(const char* message);
    void setStatusMessage(const char* message, lv_color_t color,
                         const lv_font_t* font = &lv_font_montserrat_20,
                         uint32_t timeoutMs = 0);
    void clearStatusMessage();
    void setDefaultMessageTimeout(uint32_t timeoutMs);
    
    void startStatusTimer(int intervalMs);
    void stopStatusTimer();
    
    // Sistema de popup
    /**
     * Cria overlay escuro para popup, respeitando a StatusBar.
     * REGRA: Toda tela que precisar de popup customizado DEVE usar este metodo
     * como parent. Garante tamanho e posicao corretos em todas as telas.
     * @return Ponteiro para o overlay criado (filho do screen deste ButtonManager)
     */
    lv_obj_t* createPopupOverlay();

    void showPopup(const char* title, const char* message,
                  PopupType type = POPUP_INFO, bool showCancel = false,
                  std::function<void(PopupResult)> callback = nullptr);
    void closePopup();
    PopupResult getLastPopupResult() const { return lastPopupResult; }
    
    // Utilitários
    void setScreenBackground(lv_color_t color);
    void printGridOccupancy();
};

// ============================================================================
// FUNÇÕES HELPER GLOBAIS
// ============================================================================

void initButtonManager();
void updateBtnManagerButtons(bool ignicaoOn);
void updateBtnManagerStatus();
void updateBtnManagerStatusBar();
const char* formatTime(unsigned long timeMs);
lv_color_t getStateColor(int state);
void createDefaultJornadaButtons();

#endif // BUTTON_MANAGER_H