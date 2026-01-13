# ============================================================================
# CMakeLists.txt - Teclado de Jornada Digital
# ============================================================================
#
# Copyright (c) 2024-2026 Getscale Sistemas Embarcados
# Desenvolvido por Mario Stanski Jr
#
# ============================================================================

# Coleta todos os arquivos fonte recursivamente
FILE(GLOB_RECURSE app_sources
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Lista de diretorios de include
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/config
    ${CMAKE_SOURCE_DIR}/include/interfaces
    ${CMAKE_SOURCE_DIR}/include/utils
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/services/ignicao
    ${CMAKE_SOURCE_DIR}/include/services/jornada
    ${CMAKE_SOURCE_DIR}/include/ui/common
    ${CMAKE_SOURCE_DIR}/include/ui/widgets
    ${CMAKE_SOURCE_DIR}/lib/lvgl
)

# Registra o componente
idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES
        driver
        esp_timer
        joltwallet__littlefs
        freertos
)

# Adiciona flags de compilacao
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

# Define versao no codigo
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    APP_VERSION_MAJOR=1
    APP_VERSION_MINOR=0
    APP_VERSION_PATCH=0
)
