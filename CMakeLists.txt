cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 1.5.1)
set(toolchainVersion 13_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(main C CXX ASM)

pico_sdk_init()

# Fontes atuais do projeto.
# Novos módulos serão adicionados aqui gradualmente.
set(PROJECT_SOURCES
    main.c
    src/ssd1306_i2c.c
    src/utils.c
    src/inputs.c
    src/fsm.c
    src/telemetry.c
    src/oled_ui.c
    src/buzzer.c
    src/rgb_matrix.c
    src/debug_log.c
)

add_executable(main
    ${PROJECT_SOURCES}
)

pico_set_program_name(main "main")
pico_set_program_version(main "0.1")

pico_generate_pio_header(main
    ${CMAKE_CURRENT_LIST_DIR}/pio/ws2818b.pio
)

# Saída padrão
pico_enable_stdio_uart(main 0)
pico_enable_stdio_usb(main 1)

target_link_libraries(main
    pico_stdlib
    hardware_adc
    hardware_i2c
    hardware_pio
    hardware_pwm
    hardware_clocks
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
)

target_include_directories(main PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/inc
    ${CMAKE_CURRENT_LIST_DIR}/src
)

pico_add_extra_outputs(main)