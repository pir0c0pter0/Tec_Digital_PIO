/**
 * ============================================================================
 * INTERFACE DE TELA
 * ============================================================================
 *
 * Abstrai o gerenciamento de telas/paginas da interface.
 *
 * ============================================================================
 */

#ifndef I_SCREEN_H
#define I_SCREEN_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus

/**
 * Tipos de tela disponiveis
 */
enum class ScreenType : uint8_t {
    SPLASH = 0,
    NUMPAD,
    JORNADA,
    SETTINGS,
    OTA,
    RPM,
    MAX_SCREENS
};

/**
 * Interface abstrata para uma tela
 */
class IScreen {
public:
    virtual ~IScreen() = default;

    /**
     * Obtem o tipo da tela
     * @return Tipo da tela
     */
    virtual ScreenType getType() const = 0;

    /**
     * Cria os elementos da tela
     */
    virtual void create() = 0;

    /**
     * Destroi os elementos da tela
     */
    virtual void destroy() = 0;

    /**
     * Verifica se a tela esta criada
     * @return true se criada
     */
    virtual bool isCreated() const = 0;

    /**
     * Atualiza a tela (chamado periodicamente)
     */
    virtual void update() = 0;

    /**
     * Chamado quando a tela ganha foco
     */
    virtual void onEnter() = 0;

    /**
     * Chamado quando a tela perde foco
     */
    virtual void onExit() = 0;

    /**
     * Obtem o objeto LVGL da tela
     * @return Ponteiro para o lv_obj_t da tela (ou nullptr)
     */
    virtual lv_obj_t* getLvScreen() const = 0;

    /**
     * Reseta o estado da tela sem deletar objetos LVGL.
     * Usado quando o LVGL vai auto-deletar a tela antiga
     * (ex: lv_scr_load_anim com auto_del=true).
     */
    virtual void invalidate() = 0;
};

/**
 * Interface abstrata para gerenciador de telas
 */
class IScreenManager {
public:
    virtual ~IScreenManager() = default;

    /**
     * Inicializa o gerenciador
     */
    virtual void init() = 0;

    /**
     * Navega para uma tela
     * @param type Tipo da tela destino
     */
    virtual void navigateTo(ScreenType type) = 0;

    /**
     * Obtem a tela atual
     * @return Tipo da tela atual
     */
    virtual ScreenType getCurrentScreen() const = 0;

    /**
     * Volta para a tela anterior
     * @return true se conseguiu voltar
     */
    virtual bool goBack() = 0;

    /**
     * Cicla para uma tela sem usar a pilha de navegacao
     * Ideal para alternar entre telas em loop (ex: NUMPAD <-> JORNADA)
     * @param type Tipo da tela destino
     */
    virtual void cycleTo(ScreenType type) = 0;

    /**
     * Registra uma tela
     * @param screen Ponteiro para a tela
     */
    virtual void registerScreen(IScreen* screen) = 0;

    /**
     * Atualiza todas as telas ativas
     */
    virtual void update() = 0;

    /**
     * Verifica se a navegacao esta bloqueada (ex: durante OTA)
     * @return true se navegacao bloqueada
     */
    virtual bool isNavigationLocked() const = 0;

    /**
     * Bloqueia ou desbloqueia navegacao entre telas.
     * Quando bloqueada, navigateTo/goBack/cycleTo recusam trocar tela.
     * @param locked true para bloquear, false para desbloquear
     */
    virtual void setNavigationLocked(bool locked) = 0;
};

extern "C" {
#endif

// Enum C compativel
typedef enum {
    SCREEN_TYPE_SPLASH = 0,
    SCREEN_TYPE_NUMPAD,
    SCREEN_TYPE_JORNADA,
    SCREEN_TYPE_SETTINGS,
    SCREEN_TYPE_OTA,
    SCREEN_TYPE_RPM
} screen_type_t;

// Interface C para compatibilidade
void screen_manager_init(void);
void screen_navigate_to(screen_type_t type);
screen_type_t screen_get_current(void);
void screen_go_back(void);

#ifdef __cplusplus
}
#endif

#endif // I_SCREEN_H
