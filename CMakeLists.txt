cmake_minimum_required(VERSION 3.13)
set(PROJECT sine_wave_interp)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(${PROJECT} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(${PROJECT}
    sine_wave.c
)

pico_enable_stdio_uart(${PROJECT} 1)

target_link_libraries(${PROJECT} PRIVATE
    pico_stdlib
    pico_audio_i2s
    hardware_interp
)

target_compile_definitions(${PROJECT} PRIVATE
    PICO_AUDIO_I2S_MONO_INPUT=1
    PICO_AUDIO_I2S_DATA_PIN=26
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=27
)

pico_add_extra_outputs(${PROJECT})
