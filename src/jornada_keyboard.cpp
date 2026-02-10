/**
 * ============================================================================
 * TECLADO DE JORNADA - IMPLEMENTAÇÃO (COM ANIMAÇÃO PULSANTE)
 * ============================================================================
 */

#include "jornada_keyboard.h"
#include "numpad_example.h"
#include "ui/widgets/status_bar.h"
#include "esp_bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "JORNADA_KB";

// Macro para substituir millis()
#define millis() ((uint32_t)(esp_timer_get_time() / 1000ULL))

// Definições de tela (mesmas do button_manager.h)
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320
#define STATUS_BAR_HEIGHT 40
#define GRID_AREA_HEIGHT (SCREEN_HEIGHT - STATUS_BAR_HEIGHT)

#define ENABLE_PULSING_ANIMATION 0  // DESABILITADO ATÉ RESOLVER O PROBLEMA

// Declaração externa para tocar áudio
extern "C" void playAudioFile(const char* filename);

// ============================================================================
// ANIMAÇÃO
// ============================================================================

// Callback para animar a opacidade da sombra do botão (efeito pulsante)
static void anim_shadow_opa_cb(void* var, int32_t v) {
    lv_obj_t* obj = (lv_obj_t*)var;
    
    // CORREÇÃO 1: Verificar se o objeto ainda é válido
    if (!obj || !lv_obj_is_valid(obj)) {
        return;
    }
    
    // CORREÇÃO 2: Proteger com try-catch implícito através de verificação
    if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    
    lv_obj_set_style_shadow_opa(obj, v, LV_PART_MAIN);
}

// ============================================================================
// VARIÁVEIS ESTÁTICAS
// ============================================================================

JornadaKeyboard* JornadaKeyboard::instance = nullptr;
ScreenManager* ScreenManager::instance = nullptr;

// ============================================================================
// JORNADA KEYBOARD - CONSTRUTOR E DESTRUTOR
// ============================================================================

JornadaKeyboard::JornadaKeyboard() :
    btnManager(nullptr),
    statusBar_(nullptr),
    popupMotorista(nullptr),
    acaoPendente(ACAO_JORNADA) {
    
    // NOTA: A struct 'BotaoAcao' no arquivo .h correspondente
    // deve ter um novo membro: 'bool animacaoAtiva;'
    
    // Inicializar botões e seus motoristas individuais
    for (int i = 0; i < ACAO_MAX; i++) {
        botoes[i].buttonId = -1;
        botoes[i].tipo = static_cast<TipoAcao>(i);
        botoes[i].animacaoAtiva = false; // Inicializa o estado da animação
        
        // Cada botão tem seus próprios 3 motoristas
        for (int j = 0; j < 3; j++) {
            botoes[i].motoristas[j].logado = false;
            botoes[i].motoristas[j].tempoInicio = 0;
            botoes[i].indicadores[j] = nullptr; // Mantido por compatibilidade estrutural
        }
    }
}

JornadaKeyboard::~JornadaKeyboard() {
    clearKeyboard();
}

// ============================================================================
// SINGLETON
// ============================================================================

// DEPRECATED: usar instancias per-screen via new JornadaKeyboard()
JornadaKeyboard* JornadaKeyboard::getInstance() {
    if (instance == nullptr) {
        instance = new JornadaKeyboard();
    }
    return instance;
}

void JornadaKeyboard::setStatusBar(StatusBar* bar) {
    statusBar_ = bar;
}

// ============================================================================
// INICIALIZAÇÃO
// ============================================================================

void JornadaKeyboard::init() {
    btnManager = ButtonManager::getInstance();
    if (!btnManager) {
        esp_rom_printf("ERRO: ButtonManager não inicializado!");
        return;
    }

    esp_rom_printf("JornadaKeyboard inicializado");
}

void JornadaKeyboard::init(ButtonManager* mgr) {
    btnManager = mgr;
    if (!btnManager) {
        esp_rom_printf("ERRO: ButtonManager nulo passado para JornadaKeyboard!");
        return;
    }

    esp_rom_printf("JornadaKeyboard inicializado com ButtonManager externo");
}

// ============================================================================
// MÉTODOS DE CONFIGURAÇÃO DE AÇÕES
// ============================================================================

ButtonIcon JornadaKeyboard::getIconForAction(TipoAcao acao) {
    switch (acao) {
        case ACAO_JORNADA: return ICON_STEERING;
        case ACAO_REFEICAO: return ICON_FOOD;
        case ACAO_ESPERA: return ICON_CLOCK;
        case ACAO_MANOBRA: return ICON_PLAY;
        case ACAO_CARGA: return ICON_BOX;
        case ACAO_DESCARGA: return ICON_CHART;
        case ACAO_ABASTECER: return ICON_FUEL;
        case ACAO_DESCANSAR: return ICON_HOME;
        case ACAO_TRANSITO_PARADO: return ICON_PAUSE;
        case ACAO_POLICIA: return ICON_USER;
        case ACAO_PANE: return ICON_WRENCH;
        case ACAO_EMERGENCIA: return ICON_WARNING;
        default: return ICON_NONE;
    }
}

lv_color_t JornadaKeyboard::getColorForAction(TipoAcao acao) {
    switch (acao) {
        case ACAO_JORNADA: return lv_color_hex(0x00AA00);  // Verde
        case ACAO_REFEICAO: return lv_color_hex(0x0088FF);  // Azul
        case ACAO_ESPERA: return lv_color_hex(0x0088FF);  // Azul
        case ACAO_MANOBRA: return lv_color_hex(0x0088FF);  // Azul
        case ACAO_CARGA: return lv_color_hex(0xFF8800);  // Laranja
        case ACAO_DESCARGA: return lv_color_hex(0xFF8800);  // Laranja
        case ACAO_ABASTECER: return lv_color_hex(0x0088FF);  // Azul
        case ACAO_DESCANSAR: return lv_color_hex(0x0088FF);  // Azul
        case ACAO_TRANSITO_PARADO: return lv_color_hex(0xFF0000);  // Vermelho
        case ACAO_POLICIA: return lv_color_hex(0xFF0000);  // Vermelho
        case ACAO_PANE: return lv_color_hex(0xFF0000);  // Vermelho
        case ACAO_EMERGENCIA: return lv_color_hex(0xFF0000);  // Vermelho
        default: return lv_color_hex(0x666666);
    }
}

const char* JornadaKeyboard::getLabelForAction(TipoAcao acao) {
    switch (acao) {
        case ACAO_JORNADA: return "Jornada";
        case ACAO_REFEICAO: return "Refeicao";
        case ACAO_ESPERA: return "Espera";
        case ACAO_MANOBRA: return "Manobra";
        case ACAO_CARGA: return "Carga";
        case ACAO_DESCARGA: return "Descarga";
        case ACAO_ABASTECER: return "Abastecer";
        case ACAO_DESCANSAR: return "Descansar";
        case ACAO_TRANSITO_PARADO: return "Transito";
        case ACAO_POLICIA: return "Policia";
        case ACAO_PANE: return "Pane";
        case ACAO_EMERGENCIA: return "Emergencia";
        default: return "";
    }
}

// Função helper para obter o caminho da imagem para cada ação
// Função helper para obter o caminho da imagem para cada ação
const char* getImagePathForAction(TipoAcao acao) {
    switch (acao) {
        // ****** MUDANÇA PRINCIPAL AQUI ******
        // O caminho para LittleFS deve começar com "/" e ter o prefixo "A:" para LVGL
        case ACAO_JORNADA:         return "A:/jornada.png";
        case ACAO_REFEICAO:        return "A:/refeicao.png";
        case ACAO_ESPERA:          return "A:/espera.png";
        case ACAO_MANOBRA:         return "A:/manobra.png";
        case ACAO_CARGA:           return "A:/carga.png";
        case ACAO_DESCARGA:        return "A:/descarga.png";
        case ACAO_ABASTECER:       return "A:/abastecer.png";
        case ACAO_DESCANSAR:       return "A:/descansar.png";
        case ACAO_TRANSITO_PARADO: return "A:/transito.png";
        case ACAO_POLICIA:         return "A:/policia.png";
        case ACAO_PANE:            return "A:/pane.png";
        case ACAO_EMERGENCIA:      return "A:/emergencia.png";
        default:                   return nullptr;
    }
}

// ============================================================================
// CRIAÇÃO DO TECLADO
// ============================================================================

void JornadaKeyboard::createKeyboard() {
    if (!btnManager) {
        esp_rom_printf("ERRO: ButtonManager não disponível");
        return;
    }
    
    // Limpa teclado anterior se existir
    clearKeyboard();
    
    esp_rom_printf("==============================================");
    esp_rom_printf("  CRIANDO TECLADO DE JORNADA (SISTEMA ROBUSTO)");
    esp_rom_printf("==============================================");
    
    // ========================================================================
    // MÉTODO 1: CRIAÇÃO EM LOTE (RECOMENDADO)
    // ========================================================================
    
    // Definir todos os 12 botões de uma vez
    std::vector<ButtonManager::ButtonBatchDef> buttonDefs;
    
    // Layout 4x3
    int positions[12][2] = {
        {0, 0}, {1, 0}, {2, 0}, {3, 0},  // Primeira linha
        {0, 1}, {1, 1}, {2, 1}, {3, 1},  // Segunda linha
        {0, 2}, {1, 2}, {2, 2}, {3, 2}   // Terceira linha
    };
    
    TipoAcao acoes[12] = {
        ACAO_JORNADA, ACAO_REFEICAO, ACAO_ESPERA, ACAO_MANOBRA,
        ACAO_CARGA, ACAO_DESCARGA, ACAO_ABASTECER, ACAO_DESCANSAR,
        ACAO_TRANSITO_PARADO, ACAO_POLICIA, ACAO_PANE, ACAO_EMERGENCIA
    };
    
    // Preparar definições de botões para criação em lote
    // Lambda captura 'self' para resolver instancia sem getInstance()
    JornadaKeyboard* self = this;

    for (int i = 0; i < 12; i++) {
        TipoAcao acao = acoes[i];
        int x = positions[i][0];
        int y = positions[i][1];

        // Atualizar dados do botão
        botoes[acao].tipo = acao;
        botoes[acao].label = getLabelForAction(acao);
        botoes[acao].icon = getIconForAction(acao);
        botoes[acao].color = getColorForAction(acao);

        // Adicionar à lista de definições
        ButtonManager::ButtonBatchDef def;
        def.gridX = x;
        def.gridY = y;
        def.label = botoes[acao].label;
        def.icon = botoes[acao].icon;
        def.image_src = getImagePathForAction(acao);
        def.color = botoes[acao].color;
        // Lambda com captura de self (per-screen instance, sem singleton)
        def.callback = [self](int buttonId) {
            // Encontra qual acao foi clicada
            for (int i = 0; i < ACAO_MAX; i++) {
                if (self->botoes[i].buttonId == buttonId) {
                    playAudioFile("/click.mp3");
                    self->showMotoristaSelection(self->botoes[i].tipo);
                    break;
                }
            }
        };
        def.width = 1;
        def.height = 1;
        def.textColor = lv_color_hex(0xFFFFFF);
        def.textFont = &lv_font_montserrat_16;

        buttonDefs.push_back(def);
    }
    
    // Criar todos os botões em lote (com retry automático)
    esp_rom_printf("📦 Iniciando criação em lote de 12 botões de jornada...");
    std::vector<int> ids = btnManager->addButtonBatch(buttonDefs);
    
    // Mapear IDs retornados para as ações correspondentes
    for (int i = 0; i < 12; i++) {
        TipoAcao acao = acoes[i];
        botoes[acao].buttonId = ids[i];
        esp_rom_printf("  → Botão '%s' mapeado para ID %d\n", 
                     getLabelForAction(acao), ids[i]);
    }
    
    // ========================================================================
    // VERIFICAÇÃO DE CRIAÇÃO
    // ========================================================================
    
    esp_rom_printf("\n🔍 Verificando criação dos botões...");
    
    // Aguardar confirmação de criação (com timeout de 1 segundo)
    if (btnManager->waitForAllButtons(ids, 1000)) {
        esp_rom_printf("✅ SUCESSO: Todos os botões de jornada foram criados!");
        
        // Verificar status individual para debug (opcional)
        if (true) {  // Mudar para false se não quiser debug detalhado
            esp_rom_printf("\n📊 Status individual dos botões:");
            for (int i = 0; i < 12; i++) {
                TipoAcao acao = acoes[i];
                auto status = btnManager->getButtonCreationStatus(ids[i]);
                const char* statusStr = "";
                const char* emoji = "";
                
                switch(status) {
                    case ButtonManager::CREATION_SUCCESS:
                        statusStr = "CRIADO";
                        emoji = "✓";
                        break;
                    case ButtonManager::CREATION_PENDING:
                        statusStr = "PENDENTE";
                        emoji = "⏳";
                        break;
                    case ButtonManager::CREATION_FAILED:
                        statusStr = "FALHOU";
                        emoji = "✗";
                        break;
                }
                
                esp_rom_printf("  %s %s (ID: %d): %s\n", 
                             emoji, getLabelForAction(acao), ids[i], statusStr);
            }
        }
        
    } else {
        esp_rom_printf("⚠️ AVISO: Alguns botões podem não ter sido criados");
        esp_rom_printf("📊 Botões pendentes: %d\n", btnManager->getPendingButtonCount());
        
        // Diagnóstico de falhas
        esp_rom_printf("\n🔧 Diagnóstico de problemas:");
        int failedCount = 0;
        int pendingCount = 0;
        
        for (int i = 0; i < 12; i++) {
            TipoAcao acao = acoes[i];
            auto status = btnManager->getButtonCreationStatus(ids[i]);
            
            if (status == ButtonManager::CREATION_PENDING) {
                pendingCount++;
                esp_rom_printf("  ⏳ '%s' está PENDENTE\n", getLabelForAction(acao));
            } else if (status == ButtonManager::CREATION_FAILED) {
                failedCount++;
                esp_rom_printf("  ✗ '%s' FALHOU na criação\n", getLabelForAction(acao));
            }
        }
        
        if (pendingCount > 0) {
            esp_rom_printf("\n⏳ Aguardando %d botões pendentes...\n", pendingCount);
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Verificar novamente após espera
            pendingCount = btnManager->getPendingButtonCount();
            if (pendingCount == 0) {
                esp_rom_printf("✅ Todos os botões pendentes foram processados!");
            } else {
                esp_rom_printf("⚠ Ainda há %d botões pendentes após espera\n", pendingCount);
            }
        }
        
        if (failedCount > 0) {
            esp_rom_printf("❌ ERRO: %d botões falharam definitivamente\n", failedCount);
        }
    }
    
    // ========================================================================
    // FINALIZAÇÃO
    // ========================================================================
    
    // Atualizar todos os indicadores para mostrar estado atual
    atualizarTodosIndicadores();

    // Atualizar mensagem de status via StatusBar persistente
    if (statusBar_) {
        statusBar_->setMessage("Selecione uma acao",
                               lv_color_hex(0x888888),
                               &lv_font_montserrat_16);
    }
    
    esp_rom_printf("\n==============================================");
    esp_rom_printf("  TECLADO DE JORNADA PRONTO PARA USO");
    esp_rom_printf("==============================================\n");
}


// ============================================================================
// CRIAR INDICADORES DOS MOTORISTAS (FUNCIONALIDADE REMOVIDA)
// ============================================================================

void JornadaKeyboard::criarIndicadores(TipoAcao acao) {
    // A funcionalidade dos indicadores numéricos "1, 2, 3" foi removida.
    // Em seu lugar, uma animação pulsante é usada no próprio botão
    // para indicar que um motorista está logado na ação. A lógica para
    // iniciar e parar essa animação está centralizada na função
    // 'atualizarIndicadores'.
}


// ============================================================================
// GESTÃO DE ANIMAÇÃO
// ============================================================================

void JornadaKeyboard::iniciarAnimacaoPulsante(TipoAcao acao) {

    
    if (!btnManager) return;
    
    // CORREÇÃO 3: Verificação mais robusta do botão
    GridButton* btn = btnManager->getButton(botoes[acao].buttonId);
    
    // Verificar se o botão e seu objeto existem e são válidos
    if (!btn || !btn->obj) {
        esp_rom_printf("AVISO: Botão inválido para ação %d\n", acao);
        return;
    }
    
    // CORREÇÃO 4: Verificar validade do objeto LVGL
    if (!lv_obj_is_valid(btn->obj)) {
        esp_rom_printf("ERRO: Objeto LVGL inválido para ação %d\n", acao);
        botoes[acao].buttonId = -1; // Marcar como inválido
        return;
    }
    
    // Se animação já está ativa, não fazer nada
    if (botoes[acao].animacaoAtiva) {
        return;
    }
    
    if (bsp_display_lock(100)) {
        // CORREÇÃO 5: Verificar novamente dentro do lock
        if (!lv_obj_is_valid(btn->obj)) {
            bsp_display_unlock();
            return;
        }
        
        botoes[acao].animacaoAtiva = true;

        // Configurar o estilo da sombra que será animada
        lv_obj_set_style_shadow_width(btn->obj, 15, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(btn->obj, 5, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(btn->obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

        // CORREÇÃO 6: Criar animação com verificações adicionais
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, btn->obj);
        lv_anim_set_values(&a, LV_OPA_30, LV_OPA_COVER); // Começar com 30 ao invés de 0
        lv_anim_set_time(&a, 800);
        lv_anim_set_playback_time(&a, 800);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, anim_shadow_opa_cb);
        
        // CORREÇÃO 7: Adicionar callback de delete para limpar quando objeto é destruído
        lv_anim_set_deleted_cb(&a, [](lv_anim_t* anim) {
            // Limpar qualquer estado quando animação é deletada
            esp_rom_printf("Animação limpa automaticamente");
        });
        
        lv_anim_start(&a);
        
        bsp_display_unlock();
        
        esp_rom_printf("Animação iniciada para ação %d\n", acao);
    }
}

void JornadaKeyboard::pararAnimacaoPulsante(TipoAcao acao) {
    if (!btnManager) return;
    
    GridButton* btn = btnManager->getButton(botoes[acao].buttonId);
    
    // CORREÇÃO 8: Verificações de segurança antes de parar
    if (!btn || !btn->obj) {
        botoes[acao].animacaoAtiva = false; // Limpar flag mesmo se objeto não existe
        return;
    }
    
    // Verificar se objeto LVGL é válido
    if (!lv_obj_is_valid(btn->obj)) {
        botoes[acao].animacaoAtiva = false;
        return;
    }
    
    // Se animação não está ativa, não fazer nada
    if (!botoes[acao].animacaoAtiva) {
        return;
    }
    
    if (bsp_display_lock(100)) {
        // CORREÇÃO 9: Verificar novamente dentro do lock
        if (!lv_obj_is_valid(btn->obj)) {
            botoes[acao].animacaoAtiva = false;
            bsp_display_unlock();
            return;
        }
        
        botoes[acao].animacaoAtiva = false;
        
        // Remover a animação específica do objeto
        lv_anim_del(btn->obj, anim_shadow_opa_cb);
        
        // Resetar o estilo da sombra para o estado inicial
        lv_obj_set_style_shadow_opa(btn->obj, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(btn->obj, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(btn->obj, 0, LV_PART_MAIN);
        
        bsp_display_unlock();
        
        esp_rom_printf("Animação parada para ação %d\n", acao);
    }
}


// ============================================================================
// ATUALIZAR INDICADORES (AGORA CONTROLA A ANIMAÇÃO)
// ============================================================================

void JornadaKeyboard::atualizarIndicadores(TipoAcao acao) {
    // CORREÇÃO 10: Validação completa antes de atualizar
    if (acao < 0 || acao >= ACAO_MAX) {
        esp_rom_printf("ERRO: Ação inválida %d\n", acao);
        return;
    }
    
    if (botoes[acao].buttonId == -1) {
        esp_rom_printf("AVISO: Botão não existe para ação %d\n", acao);
        return;
    }
    
    // Verificar se o botão ainda existe no ButtonManager
    if (btnManager) {
        GridButton* btn = btnManager->getButton(botoes[acao].buttonId);
        if (!btn || !btn->obj || !lv_obj_is_valid(btn->obj)) {
            esp_rom_printf("AVISO: Botão inválido detectado para ação %d, limpando...\n", acao);
            botoes[acao].buttonId = -1;
            botoes[acao].animacaoAtiva = false;
            return;
        }
    }

    // Verificar se pelo menos um motorista está logado nesta ação
    bool algumMotoristaLogado = false;
    for (int i = 0; i < 3; i++) {
        if (botoes[acao].motoristas[i].logado) {
            algumMotoristaLogado = true;
            break;
        }
    }

    // Inicia ou para a animação com base no estado
    // Atualiza animação baseado no estado do motorista
    if (algumMotoristaLogado) {
        iniciarAnimacaoPulsante(acao);
    } else {
        pararAnimacaoPulsante(acao);
    }
}


void JornadaKeyboard::atualizarTodosIndicadores() {
    for (int i = 0; i < ACAO_MAX; i++) {
        atualizarIndicadores(static_cast<TipoAcao>(i));
    }
}

// ============================================================================
// LIMPAR TECLADO
// ============================================================================

void JornadaKeyboard::clearKeyboard() {
    if (!btnManager) return;
    
    esp_rom_printf("Limpando teclado de jornada...");
    
    closeMotoristaSelection();
    
    // CORREÇÃO 11: Parar todas as animações antes de remover botões
    for (int i = 0; i < ACAO_MAX; i++) {
        if (botoes[i].animacaoAtiva) {
            pararAnimacaoPulsante(static_cast<TipoAcao>(i));
            botoes[i].animacaoAtiva = false;
        }
    }
    
    // Pequeno delay para garantir que animações sejam limpas
    lv_task_handler();
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Agora remover os botões
    for (int i = 0; i < ACAO_MAX; i++) {
        if (botoes[i].buttonId != -1) {
            btnManager->removeButton(botoes[i].buttonId);
            botoes[i].buttonId = -1;
        }
        
        // Limpar referências aos indicadores
        for (int j = 0; j < 3; j++) {
            botoes[i].indicadores[j] = nullptr;
        }
    }
    
    esp_rom_printf("Teclado de jornada removido com segurança");
}

// ============================================================================
// POPUP DE SELEÇÃO DE MOTORISTA
// ============================================================================

void JornadaKeyboard::showMotoristaSelection(TipoAcao acao) {
    if (popupMotorista) {
        closeMotoristaSelection();
    }
    
    acaoPendente = acao;
    
    if (!bsp_display_lock(100)) return;
    
    // Overlay padronizado (respeita StatusBar)
    popupMotorista = btnManager->createPopupOverlay();
    
    // Caixa do popup
    lv_obj_t* popupBox = lv_obj_create(popupMotorista);
    lv_obj_set_size(popupBox, 350, 250);
    lv_obj_center(popupBox);
    lv_obj_set_style_bg_color(popupBox, lv_color_hex(0x3a3a3a), LV_PART_MAIN);
    lv_obj_set_style_border_width(popupBox, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(popupBox, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_radius(popupBox, 15, LV_PART_MAIN);
    lv_obj_clear_flag(popupBox, LV_OBJ_FLAG_SCROLLABLE);
    
    // Botão X (cancelar) - armazena this como user_data no obj para resolver instancia
    lv_obj_t* btnCancel = lv_btn_create(popupBox);
    lv_obj_set_size(btnCancel, 30, 30);
    lv_obj_align(btnCancel, LV_ALIGN_BOTTOM_MID, 10, 10);
    lv_obj_set_style_bg_color(btnCancel, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_radius(btnCancel, 5, LV_PART_MAIN);
    lv_obj_set_user_data(btnCancel, this);
    lv_obj_add_event_cb(btnCancel, onCancelPopupClick, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* labelX = lv_label_create(btnCancel);
    lv_label_set_text(labelX, "X");
    lv_obj_center(labelX);
    lv_obj_set_style_text_color(labelX, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(labelX, &lv_font_montserrat_16, LV_PART_MAIN);
    
    // Título
    lv_obj_t* titleLabel = lv_label_create(popupBox);
    char titleText[50];
    snprintf(titleText, sizeof(titleText), "Selecione o Motorista - %s", getLabelForAction(acao));
    lv_label_set_text(titleLabel, titleText);
    lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_color(titleLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(titleLabel, &lv_font_montserrat_18, LV_PART_MAIN);
    
    // Container dos botões de motorista
    lv_obj_t* btnContainer = lv_obj_create(popupBox);
    lv_obj_set_size(btnContainer, 320, 150);
    lv_obj_align(btnContainer, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(btnContainer, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnContainer, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(btnContainer, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btnContainer, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(btnContainer, 10, LV_PART_MAIN);
    lv_obj_clear_flag(btnContainer, LV_OBJ_FLAG_SCROLLABLE);
    
    // Criar 3 botões de motorista
    for (int i = 0; i < 3; i++) {
        lv_obj_t* btnMotorista = lv_btn_create(btnContainer);
        lv_obj_set_size(btnMotorista, 280, 40);

        // Verificar estado do motorista NESTE BOTÃO específico
        bool estaLogado = botoes[acao].motoristas[i].logado;

        // Cores simples: Verde = logado, Azul = não logado
        lv_color_t corBotao = estaLogado ? lv_color_hex(0x00AA00) : lv_color_hex(0x0088FF);
        lv_obj_set_style_bg_color(btnMotorista, corBotao, LV_PART_MAIN);

        // Armazena this como user_data no obj para resolver instancia sem getInstance()
        lv_obj_set_user_data(btnMotorista, this);
        // Adicionar callback com o índice do motorista via event user_data
        lv_obj_add_event_cb(btnMotorista, onMotoristaSelectClick, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        // Label do botão simples
        char labelText[50];
        if (estaLogado) {
            snprintf(labelText, sizeof(labelText), "Motorista %d - DESLOGAR", i + 1);
        } else {
            snprintf(labelText, sizeof(labelText), "Motorista %d - LOGAR", i + 1);
        }
        
        lv_obj_t* label = lv_label_create(btnMotorista);
        lv_label_set_text(label, labelText);
        lv_obj_center(label);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    }
    
    bsp_display_unlock();
}

void JornadaKeyboard::closeMotoristaSelection() {
    if (popupMotorista && bsp_display_lock(100)) {
        lv_obj_del(popupMotorista);
        popupMotorista = nullptr;
        bsp_display_unlock();
    }
}

// ============================================================================
// PROCESSAR AÇÃO
// ============================================================================

void JornadaKeyboard::processarAcao(int motorista, TipoAcao acao) {
    if (motorista < 0 || motorista >= 3) return;
    
    // Verificar estado do motorista NESTE BOTÃO específico
    bool estaLogado = botoes[acao].motoristas[motorista].logado;
    
    if (estaLogado) {
        // Deslogar motorista deste botão
        deslogarMotorista(acao, motorista);

        char msg[100];
        snprintf(msg, sizeof(msg), "Motorista %d: %s DESATIVADO",
                 motorista + 1, getLabelForAction(acao));

        if (statusBar_) {
            statusBar_->setMessage(msg,
                                   lv_color_hex(0xFFAA00), // Laranja
                                   &lv_font_montserrat_18,
                                   3000);
        }
        playAudioFile("/nok_click.mp3");

    } else {
        // Logar motorista neste botão
        logarMotorista(acao, motorista);

        char msg[100];
        snprintf(msg, sizeof(msg), "Motorista %d: %s ATIVADO",
                 motorista + 1, getLabelForAction(acao));

        if (statusBar_) {
            statusBar_->setMessage(msg,
                                   lv_color_hex(0x00FF00), // Verde
                                   &lv_font_montserrat_18,
                                   3000);
        }
        playAudioFile("/ok_click.mp3");
    }
    
    // Atualizar o estado visual (animação) deste botão
    atualizarIndicadores(acao);
}

// ============================================================================
// GESTÃO DE MOTORISTAS
// ============================================================================

bool JornadaKeyboard::isMotoristaLogado(TipoAcao acao, int motorista) {
    if (acao >= 0 && acao < ACAO_MAX && motorista >= 0 && motorista < 3) {
        return botoes[acao].motoristas[motorista].logado;
    }
    return false;
}

void JornadaKeyboard::logarMotorista(TipoAcao acao, int motorista) {
    if (acao >= 0 && acao < ACAO_MAX && motorista >= 0 && motorista < 3) {
        botoes[acao].motoristas[motorista].logado = true;
        botoes[acao].motoristas[motorista].tempoInicio = millis();
        
        esp_rom_printf("Motorista %d logado em %s\n", motorista + 1, getLabelForAction(acao));
    }
}

void JornadaKeyboard::deslogarMotorista(TipoAcao acao, int motorista) {
    if (acao >= 0 && acao < ACAO_MAX && motorista >= 0 && motorista < 3) {
        botoes[acao].motoristas[motorista].logado = false;
        botoes[acao].motoristas[motorista].tempoInicio = 0;
        
        esp_rom_printf("Motorista %d deslogado de %s\n", motorista + 1, getLabelForAction(acao));
    }
}

EstadoMotorista JornadaKeyboard::getEstadoMotorista(TipoAcao acao, int motorista) {
    if (acao >= 0 && acao < ACAO_MAX && motorista >= 0 && motorista < 3) {
        return botoes[acao].motoristas[motorista];
    }
    
    EstadoMotorista vazio;
    vazio.logado = false;
    vazio.tempoInicio = 0;
    return vazio;
}

// ============================================================================
// CALLBACKS
// ============================================================================

// DEPRECATED: substituido por lambda em createKeyboard(). Mantido para compatibilidade.
void JornadaKeyboard::onActionButtonClick(int buttonId) {
    ESP_LOGW(TAG, "onActionButtonClick: chamada via funcao estatica (deprecated)");
    JornadaKeyboard* jornada = getInstance();
    if (!jornada) return;

    for (int i = 0; i < ACAO_MAX; i++) {
        if (jornada->botoes[i].buttonId == buttonId) {
            playAudioFile("/click.mp3");
            jornada->showMotoristaSelection(jornada->botoes[i].tipo);
            break;
        }
    }
}

void JornadaKeyboard::onMotoristaSelectClick(lv_event_t* e) {
    int motorista = (int)(intptr_t)lv_event_get_user_data(e);

    // Resolve instancia via lv_obj user_data (per-screen, sem getInstance())
    lv_obj_t* target = lv_event_get_target(e);
    JornadaKeyboard* jornada = static_cast<JornadaKeyboard*>(lv_obj_get_user_data(target));
    if (!jornada) {
        ESP_LOGE(TAG, "onMotoristaSelectClick: null user_data no botao");
        return;
    }

    jornada->processarAcao(motorista, jornada->acaoPendente);
    jornada->closeMotoristaSelection();
}

void JornadaKeyboard::onCancelPopupClick(lv_event_t* e) {
    // Resolve instancia via lv_obj user_data (per-screen, sem getInstance())
    lv_obj_t* target = lv_event_get_target(e);
    JornadaKeyboard* jornada = static_cast<JornadaKeyboard*>(lv_obj_get_user_data(target));
    if (!jornada) {
        ESP_LOGE(TAG, "onCancelPopupClick: null user_data no botao");
        return;
    }

    playAudioFile("/nok_click.mp3");
    jornada->closeMotoristaSelection();
}

// ... O restante do arquivo (ScreenManager e funções globais) permanece inalterado ...
// ============================================================================
// SCREEN MANAGER - IMPLEMENTAÇÃO
// ============================================================================

ScreenManager::ScreenManager() : 
    telaAtual(TELA_NUMPAD),
    container(nullptr),
    numpadInstance(nullptr),
    jornadaInstance(nullptr),
    btnScreenSwitch(nullptr) {
}

ScreenManager::~ScreenManager() {
    // Limpar telas
    if (numpadInstance) {
        NumpadExample* numpad = static_cast<NumpadExample*>(numpadInstance);
        numpad->clearNumpad();
    }
    
    if (jornadaInstance) {
        jornadaInstance->clearKeyboard();
    }
}

ScreenManager* ScreenManager::getInstance() {
    if (instance == nullptr) {
        instance = new ScreenManager();
    }
    return instance;
}

void ScreenManager::init() {
    esp_rom_printf("Inicializando ScreenManager...");
    
    // Inicializar instâncias PRIMEIRO
    numpadInstance = NumpadExample::getInstance();
    NumpadExample* numpad = static_cast<NumpadExample*>(numpadInstance);
    numpad->init();
    
    jornadaInstance = JornadaKeyboard::getInstance();
    jornadaInstance->init();
    
    // Mostrar tela inicial (numpad)
    showNumpadScreen();
    
    // Criar botão com delay maior para garantir que tudo esteja pronto
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* t) {
        ScreenManager* sm = ScreenManager::getInstance();
        if (sm) {
            sm->createScreenSwitchButton();
            esp_rom_printf("Botao de troca criado apos inicializacao");
        }
        lv_timer_del(t);
    }, 500, nullptr);
    
    esp_rom_printf("ScreenManager inicializado!");
}

// Criar botão de troca de tela
void ScreenManager::createScreenSwitchButton() {
    // Sempre deletar o anterior se existir
    if (btnScreenSwitch) {
        lv_obj_del(btnScreenSwitch);
        btnScreenSwitch = nullptr;
    }
    
    // Criar novo botão
    lv_obj_t* screen = lv_scr_act();
    if (!screen) {
        esp_rom_printf("ERRO: Tela nao encontrada!");
        return;
    }
    
    // Botão de troca de tela no EXTREMO canto inferior direito
    btnScreenSwitch = lv_btn_create(screen);
    lv_obj_set_size(btnScreenSwitch, 60, 35);
    // Posição absoluta no canto direito da barra de status
    lv_obj_set_pos(btnScreenSwitch, SCREEN_WIDTH - 65, SCREEN_HEIGHT - 38);
    lv_obj_set_style_bg_color(btnScreenSwitch, lv_color_hex(0x0088FF), LV_PART_MAIN);
    lv_obj_set_style_radius(btnScreenSwitch, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(btnScreenSwitch, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(btnScreenSwitch, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    
    // Texto simples mostrando o número da tela
    lv_obj_t* label = lv_label_create(btnScreenSwitch);
    char text[16];
    snprintf(text, sizeof(text), "[%d]", telaAtual + 1);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    
    // Adicionar callback
    lv_obj_add_event_cb(btnScreenSwitch, onScreenSwitchClick, LV_EVENT_CLICKED, nullptr);
    
    // Trazer para frente
    lv_obj_move_foreground(btnScreenSwitch);
    
    esp_rom_printf("Botao criado! Posicao: %d,%d Tela: %d\n", 
                  SCREEN_WIDTH - 65, SCREEN_HEIGHT - 38, telaAtual + 1);
}

// Callback para botão de troca de tela
void ScreenManager::onScreenSwitchClick(lv_event_t* e) {
    ScreenManager* mgr = getInstance();
    if (!mgr) return;
    
    playAudioFile("/click.mp3");
    
    // Alternar entre as telas
    if (mgr->telaAtual == TELA_NUMPAD) {
        mgr->showJornadaScreen();
    } else {
        mgr->showNumpadScreen();
    }
    
    esp_rom_printf("Tela trocada para: %d\n", mgr->telaAtual);
}

void ScreenManager::showNumpadScreen() {
    esp_rom_printf("Mostrando tela de teclado numerico");
    
    // Limpar tela de jornada se existir
    if (jornadaInstance) {
        jornadaInstance->clearKeyboard();
    }
    
    // Limpar todos os botões primeiro
    ButtonManager* mgr = ButtonManager::getInstance();
    if (mgr) {
        mgr->removeAllButtons();
    }
    
    // Mostrar numpad
    if (numpadInstance) {
        NumpadExample* numpad = static_cast<NumpadExample*>(numpadInstance);
        numpad->createNumpad();
    }
    
    telaAtual = TELA_NUMPAD;
    
    // Criar o botão de troca com pequeno delay para garantir que tudo esteja pronto
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* t) {
        ScreenManager* sm = ScreenManager::getInstance();
        if (sm) {
            sm->createScreenSwitchButton();
        }
        lv_timer_del(t);
    }, 100, nullptr);
    
    esp_rom_printf("Tela numpad ativada");
}

void ScreenManager::showJornadaScreen() {
    esp_rom_printf("Mostrando tela de jornada");
    
    // Limpar numpad se existir
    if (numpadInstance) {
        NumpadExample* numpad = static_cast<NumpadExample*>(numpadInstance);
        numpad->clearNumpad();
    }
    
    // Limpar todos os botões primeiro
    ButtonManager* mgr = ButtonManager::getInstance();
    if (mgr) {
        mgr->removeAllButtons();
    }
    
    // Mostrar teclado de jornada
    if (jornadaInstance) {
        jornadaInstance->createKeyboard();
    }
    
    telaAtual = TELA_JORNADA;
    
    // Criar o botão de troca com pequeno delay para garantir que tudo esteja pronto
    lv_timer_t* timer = lv_timer_create([](lv_timer_t* t) {
        ScreenManager* sm = ScreenManager::getInstance();
        if (sm) {
            sm->createScreenSwitchButton();
        }
        lv_timer_del(t);
    }, 100, nullptr);
    
    esp_rom_printf("Tela jornada ativada");
}

void ScreenManager::toggleScreen() {
    if (telaAtual == TELA_NUMPAD) {
        showJornadaScreen();
    } else {
        showNumpadScreen();
    }
}

// ============================================================================
// FUNÇÕES HELPER GLOBAIS (extern C para acesso de main.cpp)
// ============================================================================

extern "C" void initScreenManager() {
    esp_rom_printf("Inicializando sistema de gerenciamento de telas");
    ScreenManager* mgr = ScreenManager::getInstance();
    if (mgr) {
        mgr->init();
    } else {
        esp_rom_printf("ERRO: Falha ao criar ScreenManager!");
    }
}

void showNumpadKeyboard() {
    ScreenManager* mgr = ScreenManager::getInstance();
    if (mgr) {
        mgr->showNumpadScreen();
    } else {
        // Fallback direto
        NumpadExample* numpad = NumpadExample::getInstance();
        numpad->init();
        numpad->createNumpad();
    }
}

void showJornadaKeyboard() {
    ScreenManager* mgr = ScreenManager::getInstance();
    if (mgr) {
        mgr->showJornadaScreen();
    } else {
        // Fallback direto
        JornadaKeyboard* jornada = JornadaKeyboard::getInstance();
        jornada->init();
        jornada->createKeyboard();
    }
}

// Função de teste para alternar telas via serial
void toggleKeyboard() {
    ScreenManager* mgr = ScreenManager::getInstance();
    if (mgr) {
        mgr->toggleScreen();
    }
}

// ============================================================================
// FIM DA IMPLEMENTAÇÃO
// ============================================================================