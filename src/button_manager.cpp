/**
 * ============================================================================
 * GERENCIADOR DE BOTÕES - IMPLEMENTAÇÃO COM SISTEMA ROBUSTO DE CRIAÇÃO
 * ============================================================================
 */

#include "button_manager.h"
#include "jornada_manager.h"
#include "ignicao_control.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

static const char *TAG = "BTN_MGR";

// Macros de compatibilidade Arduino -> ESP-IDF
#define millis() ((uint32_t)(esp_timer_get_time() / 1000ULL))

// ============================================================================
// DEBOUNCE - CONFIGURAÇÃO
// ============================================================================
#define BUTTON_DEBOUNCE_MS 300

// ============================================================================
// VARIÁVEIS ESTÁTICAS
// ============================================================================
ButtonManager* ButtonManager::instance = nullptr;

// ============================================================================
// CONSTRUTOR E DESTRUTOR
// ============================================================================

ButtonManager::ButtonManager() :
    screen(nullptr),
    gridContainer(nullptr),
    statusBar(nullptr),
    statusIgnicao(nullptr),
    statusTempoIgnicao(nullptr),
    statusTempoJornada(nullptr),
    statusMensagem(nullptr),
    statusUpdateTimer(nullptr),
    statusTimer(nullptr),
    messageExpireTime(0),
    activePopup(nullptr),
    lastPopupResult(POPUP_RESULT_NONE),
    nextButtonId(1),
    lastButtonClickTime_(0),
    lastButtonClickedId_(-1),
    retryTimer(nullptr) {
    
    // Inicializar configuração de mensagem
    currentMessageConfig.color = lv_color_hex(0x888888);
    currentMessageConfig.font = &lv_font_montserrat_20;
    currentMessageConfig.timeoutMs = 0;
    currentMessageConfig.hasTimeout = false;
    
    // Criar mutex para sincronização
    creationMutex = xSemaphoreCreateMutex();
    
    // Inicializar grade
    for (int x = 0; x < GRID_COLS; x++) {
        for (int y = 0; y < GRID_ROWS; y++) {
            gridOccupancy[x][y] = false;
        }
    }
}

ButtonManager::~ButtonManager() {
    if (bsp_display_lock(200)) {
        // Limpar timers LVGL (requer display lock)
        if (retryTimer) {
            lv_timer_del(retryTimer);
            retryTimer = nullptr;
        }
        if (statusUpdateTimer) {
            lv_timer_del(statusUpdateTimer);
            statusUpdateTimer = nullptr;
        }
        if (statusTimer) {
            lv_timer_del(statusTimer);
            statusTimer = nullptr;
        }

        // Limpar botoes (sem deletar objetos LVGL — a tela pai sera deletada abaixo)
        buttons.clear();

        // Deletar screen LVGL (deleta todos os filhos: gridContainer, statusBar, botoes)
        if (screen != nullptr && screen != lv_scr_act()) {
            lv_obj_del(screen);
        }
        screen = nullptr;

        bsp_display_unlock();
    }

    // Deletar mutex
    if (creationMutex) {
        vSemaphoreDelete(creationMutex);
        creationMutex = nullptr;
    }
}

// ============================================================================
// SINGLETON
// ============================================================================

ButtonManager* ButtonManager::getInstance() {
    if (instance == nullptr) {
        instance = new ButtonManager();
    }
    return instance;
}

void ButtonManager::destroyInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================

void ButtonManager::init() {
    createScreen();
}

void ButtonManager::createScreen() {
    if (bsp_display_lock(0)) {
        // Limpar estado anterior (objetos LVGL sao filhos da tela antiga,
        // serao deletados automaticamente quando a tela antiga for removida)
        buttons.clear();
        for (int x = 0; x < GRID_COLS; x++)
            for (int y = 0; y < GRID_ROWS; y++)
                gridOccupancy[x][y] = false;
        nextButtonId = 0;

        screen = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x1a1a1a), LV_PART_MAIN);
        
        gridContainer = lv_obj_create(screen);
        lv_obj_set_size(gridContainer, SCREEN_WIDTH, GRID_AREA_HEIGHT);
        lv_obj_align(gridContainer, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_bg_color(gridContainer, lv_color_hex(0x2a2a2a), LV_PART_MAIN);
        lv_obj_set_style_border_width(gridContainer, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(gridContainer, GRID_PADDING, LV_PART_MAIN);
        lv_obj_clear_flag(gridContainer, LV_OBJ_FLAG_SCROLLABLE);
        
        statusBar = lv_obj_create(screen);
        lv_obj_set_size(statusBar, SCREEN_WIDTH, STATUS_BAR_HEIGHT);
        lv_obj_align(statusBar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
        lv_obj_set_style_bg_color(statusBar, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_border_width(statusBar, 0, LV_PART_MAIN);
        lv_obj_set_style_border_side(statusBar, LV_BORDER_SIDE_TOP, LV_PART_MAIN);
        lv_obj_set_style_border_color(statusBar, lv_color_hex(0x4a4a4a), LV_PART_MAIN);
        lv_obj_set_style_border_width(statusBar, 2, LV_PART_MAIN);
        lv_obj_clear_flag(statusBar, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_t* ignContainer = lv_obj_create(statusBar);
        lv_obj_set_size(ignContainer, 30, 30);
        lv_obj_align(ignContainer, LV_ALIGN_LEFT_MID, 10, 0);
        lv_obj_set_style_radius(ignContainer, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ignContainer, lv_color_hex(0xFF0000), LV_PART_MAIN);
        lv_obj_set_style_border_width(ignContainer, 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(ignContainer, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_clear_flag(ignContainer, LV_OBJ_FLAG_SCROLLABLE);
        
        statusIgnicao = lv_label_create(ignContainer);
        lv_label_set_text(statusIgnicao, "OFF");
        lv_obj_center(statusIgnicao);
        lv_obj_set_style_text_color(statusIgnicao, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(statusIgnicao, &lv_font_montserrat_10, LV_PART_MAIN);
        
        statusTempoIgnicao = lv_label_create(statusBar);
        lv_label_set_text(statusTempoIgnicao, "");
        lv_obj_align(statusTempoIgnicao, LV_ALIGN_LEFT_MID, 55, 0);
        lv_obj_set_style_text_color(statusTempoIgnicao, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(statusTempoIgnicao, &lv_font_montserrat_12, LV_PART_MAIN);
        
        statusTempoJornada = lv_label_create(statusBar);
        lv_label_set_text(statusTempoJornada, "");
        lv_obj_align(statusTempoJornada, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_text_color(statusTempoJornada, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
        lv_obj_set_style_text_font(statusTempoJornada, &lv_font_montserrat_12, LV_PART_MAIN);
        
        statusMensagem = lv_label_create(statusBar);
        lv_label_set_text(statusMensagem, "");
        lv_obj_align(statusMensagem, LV_ALIGN_CENTER, -30, 0);
        lv_obj_set_width(statusMensagem, 250);  
        lv_label_set_long_mode(statusMensagem, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(statusMensagem, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_color(statusMensagem, lv_color_hex(0x888888), LV_PART_MAIN);
        lv_obj_set_style_text_font(statusMensagem, &lv_font_montserrat_20, LV_PART_MAIN);
        
        // Nota: NAO carregar o screen aqui -- o ScreenManager controla transicoes

        if (!statusUpdateTimer) {
            statusUpdateTimer = lv_timer_create(statusUpdateCallback, 250, this);
        }
        
        bsp_display_unlock();
        
        esp_rom_printf("Button Manager screen created");
    }
}

// ============================================================================
// SISTEMA ROBUSTO DE CRIAÇÃO DE BOTÕES
// ============================================================================

int ButtonManager::addButton(int gridX, int gridY, const char* label, ButtonIcon icon,
                             const char* image_src,
                             lv_color_t color, std::function<void(int)> callback,
                             int width, int height, lv_color_t textColor, 
                             const lv_font_t* textFont) {
    
    // Tomar mutex
    if (xSemaphoreTake(creationMutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        esp_rom_printf("ERRO: Não foi possível obter mutex para criação de botão");
        return -1;
    }
    
    // Criar estrutura do botão pendente
    PendingButton pending;
    pending.gridX = gridX;
    pending.gridY = gridY;
    pending.label = label;
    pending.icon = icon;
    pending.image_src = image_src;
    pending.color = color;
    pending.callback = callback;
    pending.width = width;
    pending.height = height;
    pending.textColor = textColor;
    pending.textFont = textFont;
    pending.assignedId = nextButtonId++;
    pending.retryCount = 0;
    
    // Tentar criar imediatamente
    int createdId = addButtonInternal(pending);
    
    if (createdId > 0) {
        esp_rom_printf("✓ Botão '%s' criado com sucesso (ID: %d)\n", label, createdId);
        xSemaphoreGive(creationMutex);
        return createdId;
    }
    
    // Se falhou, adicionar à lista de pendentes
    esp_rom_printf("⚠ Botão '%s' adicionado à fila de retry\n", label);
    pendingButtons.push_back(pending);
    
    // Iniciar timer de retry se não estiver rodando
    if (!retryTimer) {
        retryTimer = lv_timer_create(retryTimerCallback, RETRY_DELAY_MS, this);
    }
    
    xSemaphoreGive(creationMutex);
    
    // Retorna o ID reservado (mesmo que ainda não criado)
    return pending.assignedId;
}

int ButtonManager::addButtonInternal(const PendingButton& btn) {
    // Validações básicas
    if (!isPositionValid(btn.gridX, btn.gridY) || 
        !isPositionValid(btn.gridX + btn.width - 1, btn.gridY + btn.height - 1)) {
        esp_rom_printf("ERRO: Posição inválida para botão '%s'\n", btn.label);
        return -1;
    }
    
    if (!isGridPositionFree(btn.gridX, btn.gridY, btn.width, btn.height)) {
        esp_rom_printf("AVISO: Posição ocupada para botão '%s', limpando...\n", btn.label);
        markGridPosition(btn.gridX, btn.gridY, btn.width, btn.height, false);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Tentar obter lock do display com timeout maior
    if (!bsp_display_lock(200)) {
        esp_rom_printf("ERRO: Não foi possível obter lock do display para '%s'\n", btn.label);
        return -1;
    }
    
    // Verificar se o display está pronto
    if (!gridContainer || !lv_obj_is_valid(gridContainer)) {
        esp_rom_printf("ERRO: Container de grade não está pronto!");
        bsp_display_unlock();
        return -1;
    }
    
    // Criar novo botão
    GridButton newButton;
    newButton.id = btn.assignedId;
        newButton.gridX = btn.gridX;
        newButton.gridY = btn.gridY;
        newButton.width = btn.width;
        newButton.height = btn.height;
        newButton.label = btn.label;
        newButton.icon = btn.icon;
        newButton.color = btn.color;
        newButton.callback = btn.callback;
        newButton.enabled = true;
        
        // Calcular posição real
        int realX = btn.gridX * (BUTTON_WIDTH + BUTTON_MARGIN);
        int realY = btn.gridY * (BUTTON_HEIGHT + BUTTON_MARGIN);
        int realWidth = (BUTTON_WIDTH * btn.width) + (BUTTON_MARGIN * (btn.width - 1));
        int realHeight = (BUTTON_HEIGHT * btn.height) + (BUTTON_MARGIN * (btn.height - 1));
        
        // Criar botão LVGL
        newButton.obj = lv_btn_create(gridContainer);
        if (!newButton.obj) {
            esp_rom_printf("ERRO: Falha ao criar objeto LVGL para '%s'\n", btn.label);
            bsp_display_unlock();
            return -1;
        }
        
        // Configurar botão
        lv_obj_set_pos(newButton.obj, realX, realY);
        lv_obj_set_size(newButton.obj, realWidth, realHeight);
        lv_obj_set_style_bg_color(newButton.obj, btn.color, LV_PART_MAIN);
        lv_obj_set_style_radius(newButton.obj, 10, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(newButton.obj, LV_OPA_COVER, LV_PART_MAIN);
        
        lv_obj_set_flex_flow(newButton.obj, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(newButton.obj, LV_FLEX_ALIGN_CENTER, 
                             LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // Adicionar imagem ou ícone
        bool image_loaded = false;
        if (btn.image_src != nullptr && strlen(btn.image_src) > 0) {
            lv_img_header_t header;
            lv_res_t res = lv_img_decoder_get_info(btn.image_src, &header);
            
            if (res == LV_RES_OK && header.w > 0 && header.h > 0) {
                lv_obj_t* img = lv_img_create(newButton.obj);
                lv_img_set_src(img, btn.image_src);
                lv_obj_set_style_bg_opa(img, LV_OPA_TRANSP, LV_PART_MAIN);
                lv_obj_set_style_border_width(img, 0, LV_PART_MAIN);
                lv_obj_set_style_pad_all(img, 0, LV_PART_MAIN);
                lv_obj_align(img, LV_ALIGN_CENTER, 0, -10);
                image_loaded = true;
                esp_rom_printf("✓ Imagem carregada: %s\n", btn.image_src);
            }
        }
        
        if (!image_loaded && btn.icon != ICON_NONE) {
            createIconForButton(btn.icon, newButton.obj, btn.textColor, &lv_font_montserrat_38);
        }
        
        // Adicionar label
        lv_obj_t* labelObj = lv_label_create(newButton.obj);
        lv_label_set_text(labelObj, btn.label);
        lv_obj_set_style_text_color(labelObj, btn.textColor, LV_PART_MAIN);
        lv_obj_set_style_text_font(labelObj, btn.textFont, LV_PART_MAIN);
        lv_obj_align(labelObj, LV_ALIGN_BOTTOM_MID, 0, -5);
        
        // Armazenar ponteiro do ButtonManager no objeto LVGL para isolamento entre telas
        lv_obj_set_user_data(newButton.obj, this);

        // Adicionar callback
        lv_obj_add_event_cb(newButton.obj, buttonEventHandler, LV_EVENT_CLICKED,
                           (void*)(intptr_t)newButton.id);
        
        // Marcar posição como ocupada
        markGridPosition(btn.gridX, btn.gridY, btn.width, btn.height, true);
        
        // Adicionar ao vetor de botões
        buttons.push_back(newButton);
        
        bsp_display_unlock();
        
        // Verificar se foi criado corretamente
        if (verifyButtonCreation(newButton.id)) {
            return newButton.id;
    } else {
        esp_rom_printf("ERRO: Botão '%s' criado mas verificação falhou\n", btn.label);
        return -1;
    }
}

bool ButtonManager::verifyButtonCreation(int buttonId) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it == buttons.end()) {
        return false;
    }
    
    // Verificar se o objeto LVGL é válido
    if (!it->obj || !lv_obj_is_valid(it->obj)) {
        return false;
    }
    
    // Verificar se tem filhos (label, ícone, etc)
    if (lv_obj_get_child_cnt(it->obj) == 0) {
        return false;
    }
    
    return true;
}

void ButtonManager::processPendingButtons() {
    if (xSemaphoreTake(creationMutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }
    
    std::vector<PendingButton> stillPending;
    
    for (auto& pending : pendingButtons) {
        pending.retryCount++;
        
        if (pending.retryCount > MAX_RETRY_ATTEMPTS) {
            esp_rom_printf("✗ Botão '%s' falhou após %d tentativas\n", 
                         pending.label, MAX_RETRY_ATTEMPTS);
            continue;
        }
        
        esp_rom_printf("↻ Retry %d/%d para botão '%s'\n", 
                     pending.retryCount, MAX_RETRY_ATTEMPTS, pending.label);
        
        int createdId = addButtonInternal(pending);
        
        if (createdId > 0) {
            esp_rom_printf("✓ Botão '%s' criado com sucesso após retry\n", pending.label);
        } else {
            stillPending.push_back(pending);
        }
    }
    
    pendingButtons = stillPending;
    
    // Se não há mais pendentes, parar timer
    if (pendingButtons.empty() && retryTimer) {
        lv_timer_del(retryTimer);
        retryTimer = nullptr;
        esp_rom_printf("✓ Todos os botões foram criados com sucesso!");
    }
    
    xSemaphoreGive(creationMutex);
}

void ButtonManager::retryTimerCallback(lv_timer_t* timer) {
    ButtonManager* mgr = (ButtonManager*)timer->user_data;
    if (mgr) {
        mgr->processPendingButtons();
    }
}

std::vector<int> ButtonManager::addButtonBatch(const std::vector<ButtonBatchDef>& buttonDefs) {
    std::vector<int> ids;
    
    esp_rom_printf("Criando lote de %d botões...\n", buttonDefs.size());
    
    for (const auto& def : buttonDefs) {
        int id = addButton(def.gridX, def.gridY, def.label, def.icon,
                          def.image_src, def.color, def.callback,
                          def.width, def.height, def.textColor, def.textFont);
        ids.push_back(id);
    }
    
    // Aguardar processamento de pendentes
    int waitTime = 0;
    while (getPendingButtonCount() > 0 && waitTime < 2000) {
        vTaskDelay(pdMS_TO_TICKS(50));
        waitTime += 50;
    }
    
    if (getPendingButtonCount() == 0) {
        esp_rom_printf("✓ Lote de botões criado com sucesso!");
    } else {
        esp_rom_printf("⚠ %d botões ainda pendentes após timeout\n", getPendingButtonCount());
    }
    
    return ids;
}

bool ButtonManager::waitForAllButtons(const std::vector<int>& buttonIds, int timeoutMs) {
    unsigned long startTime = millis();
    
    while ((millis() - startTime) < timeoutMs) {
        bool allCreated = true;
        
        for (int id : buttonIds) {
            if (!verifyButtonCreation(id)) {
                allCreated = false;
                break;
            }
        }
        
        if (allCreated) {
            return true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    return false;
}

ButtonManager::CreationStatus ButtonManager::getButtonCreationStatus(int buttonId) {
    // Verificar se está criado
    if (verifyButtonCreation(buttonId)) {
        return CREATION_SUCCESS;
    }
    
    // Verificar se está pendente
    auto it = std::find_if(pendingButtons.begin(), pendingButtons.end(),
                          [buttonId](const PendingButton& btn) { 
                              return btn.assignedId == buttonId; 
                          });
    
    if (it != pendingButtons.end()) {
        return CREATION_PENDING;
    }
    
    return CREATION_FAILED;
}

// ============================================================================
// GERENCIAMENTO DE BOTÕES - MÉTODOS EXISTENTES
// ============================================================================

bool ButtonManager::removeButton(int buttonId) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it == buttons.end()) {
        return false;
    }
    
    if (bsp_display_lock(100)) {
        markGridPosition(it->gridX, it->gridY, it->width, it->height, false);
        
        if (it->obj) {
            lv_obj_del(it->obj);
        }
        
        buttons.erase(it);
        
        bsp_display_unlock();
        
        esp_rom_printf("Button removed: ID=%d\n", buttonId);
        return true;
    }
    
    return false;
}

void ButtonManager::removeAllButtons() {
    if (bsp_display_lock(100)) {
        for (auto& btn : buttons) {
            if (btn.obj) {
                lv_obj_del(btn.obj);
            }
        }
        buttons.clear();
        
        for (int x = 0; x < GRID_COLS; x++) {
            for (int y = 0; y < GRID_ROWS; y++) {
                gridOccupancy[x][y] = false;
            }
        }
        
        bsp_display_unlock();
        esp_rom_printf("All buttons removed");
    }
}

bool ButtonManager::setButtonEnabled(int buttonId, bool enabled) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it == buttons.end()) {
        return false;
    }
    
    if (bsp_display_lock(100)) {
        it->enabled = enabled;
        if (it->obj) {
            if (enabled) {
                lv_obj_clear_state(it->obj, LV_STATE_DISABLED);
                lv_obj_set_style_bg_opa(it->obj, LV_OPA_COVER, LV_PART_MAIN);
            } else {
                lv_obj_add_state(it->obj, LV_STATE_DISABLED);
                lv_obj_set_style_bg_opa(it->obj, LV_OPA_50, LV_PART_MAIN);
            }
        }
        bsp_display_unlock();
        return true;
    }
    
    return false;
}

bool ButtonManager::setButtonLabel(int buttonId, const char* label) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it == buttons.end() || !it->obj) {
        return false;
    }
    
    if (bsp_display_lock(100)) {
        it->label = label;
        
        uint32_t child_count = lv_obj_get_child_cnt(it->obj);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* child = lv_obj_get_child(it->obj, i);
            if (lv_obj_check_type(child, &lv_label_class)) {
                lv_label_set_text(child, label);
                break;
            }
        }
        
        bsp_display_unlock();
        return true;
    }
    
    return false;
}

bool ButtonManager::setButtonIcon(int buttonId, ButtonIcon icon) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it == buttons.end() || !it->obj) {
        return false;
    }
    
    if (bsp_display_lock(100)) {
        it->icon = icon;
        
        uint32_t child_count = lv_obj_get_child_cnt(it->obj);
        lv_obj_t* labelObj = nullptr;
        
        for (int32_t i = child_count - 1; i >= 0; i--) {
            lv_obj_t* child = lv_obj_get_child(it->obj, i);
            if (lv_obj_check_type(child, &lv_label_class)) {
                const char* text = lv_label_get_text(child);
                if (text && strlen(text) > 2) {
                    labelObj = child;
                } else {
                    lv_obj_del(child);
                }
            }
        }
        
        if (icon != ICON_NONE) {
            lv_color_t textColor = lv_obj_get_style_text_color(labelObj, LV_PART_MAIN);
            const lv_font_t* textFont = lv_obj_get_style_text_font(labelObj, LV_PART_MAIN);
            
            lv_obj_t* iconObj = createIconForButton(icon, it->obj, textColor, textFont);
            if (iconObj && labelObj) {
                lv_obj_move_to_index(iconObj, 0);
            }
        }
        
        bsp_display_unlock();
        return true;
    }
    
    return false;
}

bool ButtonManager::setButtonColor(int buttonId, lv_color_t color) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it == buttons.end() || !it->obj) {
        return false;
    }
    
    if (bsp_display_lock(100)) {
        it->color = color;
        lv_obj_set_style_bg_color(it->obj, color, LV_PART_MAIN);
        bsp_display_unlock();
        return true;
    }
    
    return false;
}

GridButton* ButtonManager::getButton(int buttonId) {
    auto it = std::find_if(buttons.begin(), buttons.end(),
                          [buttonId](const GridButton& btn) { return btn.id == buttonId; });
    
    if (it != buttons.end()) {
        return &(*it);
    }
    return nullptr;
}

// ============================================================================
// MÉTODOS PRIVADOS - GRADE
// ============================================================================

bool ButtonManager::isGridPositionFree(int x, int y, int width, int height) {
    for (int cx = x; cx < x + width; cx++) {
        for (int cy = y; cy < y + height; cy++) {
            if (!isPositionValid(cx, cy) || gridOccupancy[cx][cy]) {
                return false;
            }
        }
    }
    return true;
}

void ButtonManager::markGridPosition(int x, int y, int width, int height, bool occupied) {
    for (int cx = x; cx < x + width; cx++) {
        for (int cy = y; cy < y + height; cy++) {
            if (isPositionValid(cx, cy)) {
                gridOccupancy[cx][cy] = occupied;
            }
        }
    }
}

bool ButtonManager::findFreePosition(int width, int height, int* outX, int* outY) {
    for (int y = 0; y <= GRID_ROWS - height; y++) {
        for (int x = 0; x <= GRID_COLS - width; x++) {
            if (isGridPositionFree(x, y, width, height)) {
                if (outX) *outX = x;
                if (outY) *outY = y;
                return true;
            }
        }
    }
    return false;
}

bool ButtonManager::isPositionValid(int x, int y) {
    return x >= 0 && x < GRID_COLS && y >= 0 && y < GRID_ROWS;
}

// ============================================================================
// CRIAÇÃO DE ÍCONES
// ============================================================================

lv_obj_t* ButtonManager::createIconForButton(ButtonIcon icon, lv_obj_t* parent, 
                                               lv_color_t textColor, const lv_font_t* iconFont) {
    if (icon == ICON_NONE) return nullptr;
    
    lv_obj_t* iconLabel = lv_label_create(parent);
    lv_label_set_text(iconLabel, getIconText(icon));
    
    lv_obj_set_style_text_color(iconLabel, textColor, LV_PART_MAIN);
    
    if (iconFont == &lv_font_montserrat_20) {
        lv_obj_set_style_text_font(iconLabel, &lv_font_montserrat_24, LV_PART_MAIN);
    } else if (iconFont == &lv_font_montserrat_14) {
        lv_obj_set_style_text_font(iconLabel, &lv_font_montserrat_18, LV_PART_MAIN);
    } else {
        lv_obj_set_style_text_font(iconLabel, iconFont, LV_PART_MAIN);
    }
    
    return iconLabel;
}

const char* ButtonManager::getIconText(ButtonIcon icon) {
    switch (icon) {
        case ICON_STEERING: return LV_SYMBOL_LOOP;
        case ICON_CLOCK: return LV_SYMBOL_BELL;
        case ICON_FOOD: return LV_SYMBOL_LIST;
        case ICON_FUEL: return LV_SYMBOL_BATTERY_3;
        case ICON_TRUCK: return LV_SYMBOL_DRIVE;
        case ICON_BOX: return LV_SYMBOL_DIRECTORY;
        case ICON_WRENCH: return LV_SYMBOL_SETTINGS;
        case ICON_USER: return LV_SYMBOL_DUMMY"U";
        case ICON_MAP: return LV_SYMBOL_GPS;
        case ICON_SETTINGS: return LV_SYMBOL_SETTINGS;
        case ICON_POWER: return LV_SYMBOL_POWER;
        case ICON_PAUSE: return LV_SYMBOL_PAUSE;
        case ICON_PLAY: return LV_SYMBOL_PLAY;
        case ICON_STOP: return LV_SYMBOL_STOP;
        case ICON_CHECK: return LV_SYMBOL_OK;
        case ICON_CANCEL: return LV_SYMBOL_CLOSE;
        case ICON_WARNING: return LV_SYMBOL_WARNING;
        case ICON_INFO: return LV_SYMBOL_DUMMY"i";
        case ICON_HOME: return LV_SYMBOL_HOME;
        case ICON_CHART: return LV_SYMBOL_DUMMY"#";
        default: return "";
    }
}

// ============================================================================
// BARRA DE STATUS
// ============================================================================

void ButtonManager::updateStatusBar(const StatusBarData& data) {
    if (!statusBar) return;
    
    if (bsp_display_lock(100)) {
        if (statusIgnicao) {
            lv_obj_t* parent = lv_obj_get_parent(statusIgnicao);
            if (data.ignicaoOn) {
                lv_obj_set_style_bg_color(parent, lv_color_hex(0x00FF00), LV_PART_MAIN);
                lv_label_set_text(statusIgnicao, "ON");
            } else {
                lv_obj_set_style_bg_color(parent, lv_color_hex(0xFF0000), LV_PART_MAIN);
                lv_label_set_text(statusIgnicao, "OFF");
            }
        }
        
        if (statusTempoIgnicao) {
            if (data.ignicaoOn && data.tempoIgnicao > 0) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "Ignicao: %s", formatTime(data.tempoIgnicao));
                lv_label_set_text(statusTempoIgnicao, buffer);
            } else {
                lv_label_set_text(statusTempoIgnicao, "");
            }
        }
        
        if (statusTempoJornada) {
            if (data.tempoJornada > 0) {
                char buffer[32];
                snprintf(buffer, sizeof(buffer), "Jornada M1: %s", formatTime(data.tempoJornada));
                lv_label_set_text(statusTempoJornada, buffer);
            } else {
                lv_label_set_text(statusTempoJornada, "");
            }
        }
        
        if (statusMensagem) {
            if (data.mensagemExtra && strlen(data.mensagemExtra) > 0) {
                lv_label_set_text(statusMensagem, data.mensagemExtra);
            }
        }
        
        bsp_display_unlock();
    }
}

// ============================================================================
// SISTEMA DE MENSAGENS
// ============================================================================

void ButtonManager::statusUpdateCallback(lv_timer_t* timer) {
    ButtonManager* mgr = (ButtonManager*)timer->user_data;
    if (!mgr) return;

    // Lógica para expiração da mensagem de status
    if (mgr->messageExpireTime > 0 && millis() >= mgr->messageExpireTime) {
        mgr->clearStatusMessage();
        mgr->messageExpireTime = 0;
    }
}

void ButtonManager::setStatusMessage(const char* message) {
    setStatusMessage(message, lv_color_hex(0x888888), &lv_font_montserrat_20, 0);
}

void ButtonManager::setStatusMessage(const char* message, 
                                    lv_color_t color,
                                    const lv_font_t* font,
                                    uint32_t timeoutMs) {
    if (!statusMensagem) return;
    
    if (bsp_display_lock(100)) {
        lv_label_set_text(statusMensagem, message ? message : "");
        lv_obj_set_style_text_color(statusMensagem, color, LV_PART_MAIN);
        lv_obj_set_style_text_font(statusMensagem, font, LV_PART_MAIN);
        
        if (timeoutMs > 0 && message && strlen(message) > 0) {
            messageExpireTime = millis() + timeoutMs;
        } else {
            messageExpireTime = 0;
        }
        
        bsp_display_unlock();
    }
}

void ButtonManager::clearStatusMessage() {
    if (!statusMensagem) return;
    
    messageExpireTime = 0;
    
    if (bsp_display_lock(100)) {
        lv_label_set_text(statusMensagem, "");
        bsp_display_unlock();
    }
}

void ButtonManager::setDefaultMessageTimeout(uint32_t timeoutMs) {
    currentMessageConfig.timeoutMs = timeoutMs;
    currentMessageConfig.hasTimeout = (timeoutMs > 0);
}

void ButtonManager::statusTimerHandler(lv_timer_t* timer) {
}

void ButtonManager::startStatusTimer(int intervalMs) {
    if (statusTimer) {
        lv_timer_del(statusTimer);
    }
    statusTimer = lv_timer_create(statusTimerHandler, intervalMs, this);
}

void ButtonManager::stopStatusTimer() {
    if (statusTimer) {
        lv_timer_del(statusTimer);
        statusTimer = nullptr;
    }
}

// ============================================================================
// SISTEMA DE POPUP
// ============================================================================

lv_obj_t* ButtonManager::createPopupOverlay() {
    lv_obj_t* overlay = lv_obj_create(screen);
    lv_obj_set_size(overlay, SCREEN_WIDTH, GRID_AREA_HEIGHT);
    lv_obj_align(overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    return overlay;
}

void ButtonManager::showPopup(const char* title, const char* message,
                              PopupType type, bool showCancel,
                              std::function<void(PopupResult)> callback) {

    if (activePopup) {
        closePopup();
    }

    if (!bsp_display_lock(100)) return;

    popupCallback = callback;
    lastPopupResult = POPUP_RESULT_NONE;

    // Overlay padronizado (respeita StatusBar)
    activePopup = createPopupOverlay();
    
    // Caixa do popup
    lv_obj_t* popupBox = lv_obj_create(activePopup);
    lv_obj_set_size(popupBox, 400, 250);
    lv_obj_center(popupBox);
    lv_obj_set_style_bg_color(popupBox, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_border_width(popupBox, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(popupBox, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(popupBox, 15, LV_PART_MAIN);
    lv_obj_clear_flag(popupBox, LV_OBJ_FLAG_SCROLLABLE);
    
    // Container do header
    lv_obj_t* headerContainer = lv_obj_create(popupBox);
    lv_obj_set_size(headerContainer, 370, 50);
    lv_obj_align(headerContainer, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_opa(headerContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(headerContainer, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(headerContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(headerContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(headerContainer, LV_OBJ_FLAG_SCROLLABLE);
    
    // Ícone do popup
    lv_obj_t* iconLabel = lv_label_create(headerContainer);
    const char* iconText = "";
    lv_color_t iconColor = lv_color_hex(0xFFFFFF);
    
    switch (type) {
        case POPUP_INFO:
            iconText = LV_SYMBOL_DUMMY"i";
            iconColor = lv_color_hex(0x00AAFF);
            break;
        case POPUP_WARNING:
            iconText = LV_SYMBOL_WARNING;
            iconColor = lv_color_hex(0xFFAA00);
            break;
        case POPUP_ERROR:
            iconText = LV_SYMBOL_CLOSE;
            iconColor = lv_color_hex(0xFF0000);
            break;
        case POPUP_QUESTION:
            iconText = LV_SYMBOL_DUMMY"?";
            iconColor = lv_color_hex(0x00FF00);
            break;
        case POPUP_SUCCESS:
            iconText = LV_SYMBOL_OK;             
            iconColor = lv_color_hex(0x00FF00);
            break;
    }
    
    lv_label_set_text(iconLabel, iconText);
    lv_obj_set_style_text_color(iconLabel, iconColor, LV_PART_MAIN);
    lv_obj_set_style_text_font(iconLabel, &lv_font_montserrat_28, LV_PART_MAIN);
    
    // Título do popup
    lv_obj_t* titleLabel = lv_label_create(headerContainer);
    lv_label_set_text(titleLabel, title);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_set_style_pad_left(titleLabel, 10, LV_PART_MAIN);
    
    // Mensagem do popup
    lv_obj_t* msgLabel = lv_label_create(popupBox);
    lv_label_set_text(msgLabel, message);
    lv_label_set_long_mode(msgLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msgLabel, 370);
    lv_obj_align(msgLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_align(msgLabel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(msgLabel, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_text_font(msgLabel, &lv_font_montserrat_18, LV_PART_MAIN);
    
    // Container dos botões
    lv_obj_t* btnContainer = lv_obj_create(popupBox);
    lv_obj_set_size(btnContainer, 370, 60);
    lv_obj_align(btnContainer, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_opa(btnContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnContainer, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btnContainer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btnContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btnContainer, 20, LV_PART_MAIN);
    lv_obj_clear_flag(btnContainer, LV_OBJ_FLAG_SCROLLABLE);
    
    // Botão OK
    lv_obj_t* btnOk = lv_btn_create(btnContainer);
    lv_obj_set_size(btnOk, 120, 45);
    lv_obj_set_style_bg_color(btnOk, lv_color_hex(0x00AA00), LV_PART_MAIN);
    lv_obj_set_user_data(btnOk, this);
    lv_obj_add_event_cb(btnOk, popupButtonHandler, LV_EVENT_CLICKED, (void*)(intptr_t)POPUP_RESULT_OK);
    
    lv_obj_t* labelOk = lv_label_create(btnOk);
    lv_label_set_text(labelOk, "OK");
    lv_obj_center(labelOk);
    lv_obj_set_style_text_color(labelOk, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelOk, &lv_font_montserrat_20, LV_PART_MAIN);
    
    if (showCancel) {
        // Botão Cancelar
        lv_obj_t* btnCancel = lv_btn_create(btnContainer);
        lv_obj_set_size(btnCancel, 120, 45);
        lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0xAA0000), LV_PART_MAIN);
        lv_obj_set_user_data(btnCancel, this);
        lv_obj_add_event_cb(btnCancel, popupButtonHandler, LV_EVENT_CLICKED, (void*)(intptr_t)POPUP_RESULT_CANCEL);
        
        lv_obj_t* labelCancel = lv_label_create(btnCancel);
        lv_label_set_text(labelCancel, "Cancelar");
        lv_obj_center(labelCancel);
        lv_obj_set_style_text_color(labelCancel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(labelCancel, &lv_font_montserrat_20, LV_PART_MAIN);
    }
    
    bsp_display_unlock();
}

void ButtonManager::closePopup() {
    if (activePopup && bsp_display_lock(100)) {
        lv_obj_del(activePopup);
        activePopup = nullptr;
        bsp_display_unlock();
    }
}

// ============================================================================
// EVENT HANDLERS
// ============================================================================

void ButtonManager::buttonEventHandler(lv_event_t* e) {
    int buttonId = (int)(intptr_t)lv_event_get_user_data(e);

    // Obter o ButtonManager correto a partir do objeto LVGL (isolamento entre telas)
    lv_obj_t* target = lv_event_get_target(e);
    ButtonManager* mgr = static_cast<ButtonManager*>(lv_obj_get_user_data(target));
    if (!mgr) {
        mgr = getInstance(); // fallback para codigo legado
    }

    unsigned long currentTime = millis();

    // Debounce por instancia (cada tela tem seu proprio estado)
    if (buttonId == mgr->lastButtonClickedId_ &&
        (currentTime - mgr->lastButtonClickTime_) < BUTTON_DEBOUNCE_MS) {
        esp_rom_printf("DEBOUNCE: Ignorando clique repetido do botão ID=%d\n", buttonId);
        return;
    }

    mgr->lastButtonClickTime_ = currentTime;
    mgr->lastButtonClickedId_ = buttonId;

    GridButton* btn = mgr->getButton(buttonId);
    if (btn && btn->enabled && btn->callback) {
        esp_rom_printf("Button clicked: ID=%d, Label=%s\n", buttonId, btn->label.c_str());
        btn->callback(buttonId);
    }
}

void ButtonManager::popupButtonHandler(lv_event_t* e) {
    PopupResult result = (PopupResult)(intptr_t)lv_event_get_user_data(e);

    // Obter o ButtonManager correto a partir do objeto LVGL (isolamento entre telas)
    lv_obj_t* target = lv_event_get_target(e);
    ButtonManager* mgr = static_cast<ButtonManager*>(lv_obj_get_user_data(target));
    if (!mgr) {
        mgr = getInstance(); // fallback para codigo legado
    }

    unsigned long currentTime = millis();
    static unsigned long lastPopupClickTime = 0;

    if ((currentTime - lastPopupClickTime) < BUTTON_DEBOUNCE_MS) {
        esp_rom_printf("DEBOUNCE: Ignorando clique repetido no popup");
        return;
    }

    lastPopupClickTime = currentTime;

    mgr->lastPopupResult = result;

    if (mgr->popupCallback) {
        mgr->popupCallback(result);
    }

    mgr->closePopup();
}

// ============================================================================
// UTILITÁRIOS
// ============================================================================

void ButtonManager::setScreenBackground(lv_color_t color) {
    if (screen && bsp_display_lock(100)) {
        lv_obj_set_style_bg_color(screen, color, LV_PART_MAIN);
        bsp_display_unlock();
    }
}

void ButtonManager::printGridOccupancy() {
    esp_rom_printf("Grid Occupancy Map:");
    for (int y = 0; y < GRID_ROWS; y++) {
        esp_rom_printf("  ");
        for (int x = 0; x < GRID_COLS; x++) {
            esp_rom_printf(gridOccupancy[x][y] ? "X" : ".");
            esp_rom_printf(" ");
        }
        esp_rom_printf("\n");
    }
}

// ============================================================================
// FUNÇÕES HELPER GLOBAIS (EXEMPLO)
// ============================================================================

extern "C" void playAudioFile(const char* filename);

void initButtonManager() {
    esp_rom_printf("Inicializando Button Manager...");
    
    ButtonManager* btnManager = ButtonManager::getInstance();
    btnManager->init();
    
    esp_rom_printf("Button Manager inicializado!");
}

const char* formatTime(unsigned long timeMs) {
    static char buffer[16];
    unsigned long seconds = timeMs / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    
    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", 
                 hours, minutes % 60, seconds % 60);
    } else {
        snprintf(buffer, sizeof(buffer), "%02lu:%02lu", 
                 minutes, seconds % 60);
    }
    
    return buffer;
}

lv_color_t getStateColor(int state) {
    switch (state) {
        case 1: return lv_color_hex(0x00AA00);
        case 2: return lv_color_hex(0x0088FF);
        case 3: return lv_color_hex(0xFF8800);
        case 4: return lv_color_hex(0xFFAA00);
        case 5: return lv_color_hex(0xAA00AA);
        case 6: return lv_color_hex(0x00AAAA);
        default: return lv_color_hex(0x666666);
    }
}

void createDefaultJornadaButtons() {
    ButtonManager* mgr = ButtonManager::getInstance();
    
    mgr->addButton(0, 0, "JORNADA", ButtonIcon::ICON_TRUCK, nullptr,
                   lv_color_hex(0x00AA00), nullptr, 2, 1);
    
    mgr->addButton(2, 0, "MANOBRA", ButtonIcon::ICON_STEERING, nullptr,
                   lv_color_hex(0x0088FF), nullptr, 2, 1);
    
    mgr->addButton(0, 1, "REFEICAO", ButtonIcon::ICON_FOOD, nullptr,
                   lv_color_hex(0xFF8800), nullptr, 2, 1);
    
    mgr->addButton(2, 1, "ESPERA", ButtonIcon::ICON_CLOCK, nullptr,
                   lv_color_hex(0xFFAA00), nullptr, 2, 1);
    
    mgr->addButton(0, 2, "DESCARGA", ButtonIcon::ICON_BOX, nullptr,
                   lv_color_hex(0xAA00AA), nullptr, 2, 1);
    
    mgr->addButton(2, 2, "ABASTEC.", ButtonIcon::ICON_FUEL, nullptr,
                   lv_color_hex(0x00AAAA), nullptr, 2, 1);
}

// ============================================================================
// WRAPPERS EXTERN C PARA CHAMADAS DE main.cpp
// ============================================================================

// Variável global para acesso externo
void* btnManager = nullptr;
void* screenManager = nullptr;

extern "C" {

void buttonManagerInit(void) {
    esp_rom_printf("Inicializando Button Manager (extern C)...\n");
    ButtonManager* mgr = ButtonManager::getInstance();
    mgr->init();
    btnManager = static_cast<void*>(mgr);
    esp_rom_printf("Button Manager inicializado!\n");
}

void* buttonManagerGetInstance(void) {
    return static_cast<void*>(ButtonManager::getInstance());
}

void buttonManagerUpdateStatusBar(bool ignicaoOn, uint32_t tempoIgnicao, uint32_t tempoJornada, const char* msg) {
    ButtonManager* mgr = ButtonManager::getInstance();
    if (mgr) {
        StatusBarData data = {
            .ignicaoOn = ignicaoOn,
            .tempoIgnicao = tempoIgnicao,
            .tempoJornada = tempoJornada,
            .mensagemExtra = msg
        };
        mgr->updateStatusBar(data);
    }
}

} // extern "C"

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================