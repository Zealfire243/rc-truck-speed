# Motor controller firmware — STM32F405RG, on a Flipsky Mini FSESC6.7 Pro.
#
# One target, bench and flight alike. An earlier version of this file built
# for a Nucleo-G474RE bench rig with a HAL port planned at Phase 2b; that rig
# was dropped and there is no port. See docs/BOM.md.
#
# Requires:
#   - arm-none-eabi-gcc
#   - STM32CubeF4 v1.27+ at $CUBE_F4_PATH
#
#   export CUBE_F4_PATH=$HOME/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0
#   cmake -B build && cmake --build build
#
# The HAL is not vendored here, the same way the crawler doesn't vendor
# ESP-IDF — point CUBE_F4_PATH at your install.
#
# BRINGUP_APP is ON by default so a fresh clone builds the Phase 1 bench app
# rather than firmware that expects a whole vehicle. See docs/BUILD_PHASES.md.

cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(speed-motor-ctrl C ASM)

option(BRINGUP_APP "Build the Phase 1 bench app" ON)
set(AXLE_SELF "AXLE_FRONT" CACHE STRING "Which axle this build drives: AXLE_FRONT or AXLE_REAR")

if(NOT DEFINED ENV{CUBE_F4_PATH})
    message(FATAL_ERROR
        "CUBE_F4_PATH is not set. Point it at your STM32CubeF4 install:\n"
        "  export CUBE_F4_PATH=$HOME/STM32Cube/Repository/STM32Cube_FW_F4_V1.28.0")
endif()
set(CUBE $ENV{CUBE_F4_PATH})

# Cortex-M4F. The hard-float ABI is not optional — the FOC pipeline is float
# math in a 50 us ISR, and soft-float will not make that deadline.
#
# These flags are identical to the G474's: both parts are Cortex-M4 with the
# same single-precision FPU. The F405 runs at 168 MHz against the G4's 170 and
# has no CORDIC, which costs about a microsecond a loop — see docs/BOM.md.
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

add_compile_options(
    ${MCU_FLAGS}
    -Wall -Wextra -Wshadow -Wdouble-promotion
    -ffunction-sections -fdata-sections
    $<$<CONFIG:Release>:-O2>
    $<$<CONFIG:Debug>:-Og -g3>
)

# -Wdouble-promotion is deliberate. A stray double literal in the control ISR
# silently drags float math onto the software double path and blows the
# deadline. Let the compiler catch it.

add_link_options(
    ${MCU_FLAGS}
    -Wl,--gc-sections
    -Wl,-Map=${PROJECT_NAME}.map
    --specs=nano.specs
    -T${CMAKE_SOURCE_DIR}/STM32F405RGTx_FLASH.ld
)

add_executable(${PROJECT_NAME}
    src/main.c
    src/foc.c
    src/observer.c
    src/current_sense.c
    src/inverter.c
    src/speed_loop.c
    src/rc_input.c
    src/safety.c
    src/link.c
    $<$<BOOL:${BRINGUP_APP}>:src/bringup.c>
)

target_include_directories(${PROJECT_NAME} PRIVATE
    inc
    ${CUBE}/Drivers/STM32F4xx_HAL_Driver/Inc
    ${CUBE}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
    ${CUBE}/Drivers/CMSIS/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    STM32F405xx
    USE_HAL_DRIVER
    AXLE_SELF=${AXLE_SELF}
    $<$<BOOL:${BRINGUP_APP}>:BRINGUP_APP>
)

target_link_libraries(${PROJECT_NAME} PRIVATE m)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
)
