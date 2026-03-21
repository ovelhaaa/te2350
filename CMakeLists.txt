# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion RISCV_ZCB_RPI_2_2_0_3)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

# Initialize Pico SDK
include(pico_sdk_import.cmake)

project(te2350_effect C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

# Add source files
add_executable(te2350
    src/main.c
    src/te2350.c
    src/dsp_delay.c
    src/dsp_filters.c
    src/dsp_modulation.c
    src/dsp_pitch.c
    src/audio_driver.c
    src/audio_i2s.pio
)

# Generate PIO header
pico_generate_pio_header(te2350 ${CMAKE_CURRENT_LIST_DIR}/src/audio_i2s.pio)
pico_generate_pio_header(te2350 ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

# Include directories
target_include_directories(te2350 PRIVATE include)

# Link libraries
target_link_libraries(te2350 
    pico_stdlib
    # pico_audio_i2s # Unavailable, enable when pico-extras is added
    hardware_gpio
    hardware_clocks
    hardware_dma
    hardware_pio
)

# Enable USB Output for printf (and disable UART if desired)
pico_enable_stdio_usb(te2350 1)
pico_enable_stdio_uart(te2350 0)

# Create map/bin/hex/uf2
pico_add_extra_outputs(te2350)
