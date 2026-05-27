cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/arm-none-eabi.cmake")
project(tacometro VERSION 0.1.0 LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# Set the MCU-specific flags
add_compile_definitions(STM32F103xB USE_FULL_LL_DRIVER)

set(MCU_FLAGS "-mcpu=cortex-m3 -mthumb -mfloat-abi=soft")
separate_arguments(MCU_FLAGS)

add_compile_options(
    ${MCU_FLAGS}
    -Wall
    -Wextra
    -fdata-sections
    -ffunction-sections
)

add_link_options(
    ${MCU_FLAGS}
    -Wl,--gc-sections
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--no-warn-rwx-segments # <-- ADDED -Wl, prefix here
)

include(FetchContent)

# --- 1. Dependencies (Automated) ---
FetchContent_Declare(
  stm32f1_hal
  GIT_REPOSITORY https://github.com/STMicroelectronics/stm32f1xx-hal-driver.git
  GIT_TAG v1.1.10
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  cmsis_f1
  GIT_REPOSITORY https://github.com/STMicroelectronics/cmsis-device-f1.git
  GIT_TAG v4.3.5
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  cmsis_core
  GIT_REPOSITORY https://github.com/ARM-software/CMSIS_5.git
  GIT_TAG 5.9.0
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(stm32f1_hal cmsis_f1 cmsis_core)

# --- 2. Initial Setup: Copy Templates to 'core' ---
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bsp/system_stm32f1xx.c")
  file(COPY "${cmsis_f1_SOURCE_DIR}/Source/Templates/system_stm32f1xx.c"
      DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bsp")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bsp/startup_stm32f103xb.s")
  file(COPY "${cmsis_f1_SOURCE_DIR}/Source/Templates/gcc/startup_stm32f103xb.s"
      DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bsp")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bsp/STM32F103XB_FLASH.ld")
  file(COPY "${cmsis_f1_SOURCE_DIR}/Source/Templates/gcc/linker/STM32F103XB_FLASH.ld"
      DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/bsp")
endif()

add_library(stm32_drivers STATIC)

target_include_directories(stm32_drivers PUBLIC
    ${stm32f1_hal_SOURCE_DIR}/Inc
    ${cmsis_f1_SOURCE_DIR}/Include
    ${cmsis_core_SOURCE_DIR}/CMSIS/Core/Include
)

target_sources(stm32_drivers PRIVATE
    "${stm32f1_hal_SOURCE_DIR}/Src/stm32f1xx_ll_rcc.c"
    "${stm32f1_hal_SOURCE_DIR}/Src/stm32f1xx_ll_pwr.c"
    "${stm32f1_hal_SOURCE_DIR}/Src/stm32f1xx_ll_utils.c"
    "${stm32f1_hal_SOURCE_DIR}/Src/stm32f1xx_ll_gpio.c"
    "${stm32f1_hal_SOURCE_DIR}/Src/stm32f1xx_ll_usart.c"
    "${stm32f1_hal_SOURCE_DIR}/Src/stm32f1xx_ll_tim.c"
)

add_executable(tacometro
  src/main.c
  bsp/startup_stm32f103xb.s
  bsp/system_stm32f1xx.c
)

target_link_libraries(tacometro PRIVATE stm32_drivers)
target_link_options(tacometro PRIVATE
"-T${CMAKE_CURRENT_SOURCE_DIR}/bsp/STM32F103XB_FLASH.ld")

set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".elf")

# Use generator expressions ($<TARGET_FILE:target>) to ensure
# we always point to the actual binary produced.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY}
    -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY}
    -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Generating .hex and .bin from $<TARGET_FILE_NAME:${PROJECT_NAME}>"
)

# --- 9. Flashing with OpenOCD ---

# Find OpenOCD executable
find_program(OPENOCD_BIN openocd)

if(OPENOCD_BIN)
  # Define the OpenOCD configuration files for Nucleo-G4
  set(OPENOCD_INTERFACE "interface/stlink.cfg")
  set(OPENOCD_TARGET    "target/stm32f1x.cfg")

  add_custom_target(flash
        # Command to flash the .elf file
        COMMAND ${OPENOCD_BIN} -f ${OPENOCD_INTERFACE}
                -c "set CPUTAPID 0x2ba01477" # Compatibilidad con Blupill pirata
                -f ${OPENOCD_TARGET}
                -c "program ${PROJECT_NAME}.elf verify reset exit"
        DEPENDS ${PROJECT_NAME}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Flashing ${PROJECT_NAME}.elf to STM32F103 via OpenOCD..."
  )

else()
  message(WARNING "OpenOCD not found. 'flash' target will not be available.")
endif()
