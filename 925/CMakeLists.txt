cmake_minimum_required(VERSION 3.20)
project(925 C ASM)
option(BATCH_DEMO "Send a 2500-byte deterministic test stream" OFF)
set(HAL Drivers/STM32F1xx_HAL_Driver)
set(DEVICE Drivers/CMSIS/Device/ST/STM32F1xx)
add_executable(925
    Core/Src/main.c ../stm32/batch_link.c
    ${DEVICE}/Source/Templates/system_stm32f1xx.c
    ${DEVICE}/Source/Templates/gcc/startup_stm32f103xe.s
    ${HAL}/Src/stm32f1xx_hal.c
    ${HAL}/Src/stm32f1xx_hal_cortex.c
    ${HAL}/Src/stm32f1xx_hal_dma.c
    ${HAL}/Src/stm32f1xx_hal_gpio.c
    ${HAL}/Src/stm32f1xx_hal_spi.c
    ${HAL}/Src/stm32f1xx_hal_rcc.c
    ${HAL}/Src/stm32f1xx_hal_rcc_ex.c
    ${HAL}/Src/stm32f1xx_hal_flash.c
    ${HAL}/Src/stm32f1xx_hal_flash_ex.c
    ${HAL}/Src/stm32f1xx_hal_pwr.c)
target_include_directories(925 PRIVATE Core/Inc ../stm32 ${HAL}/Inc ${DEVICE}/Include Drivers/CMSIS/Include)
target_compile_definitions(925 PRIVATE USE_HAL_DRIVER STM32F103xE BATCH_DEMO=$<BOOL:${BATCH_DEMO}>)
target_compile_options(925 PRIVATE -mcpu=cortex-m3 -mthumb -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
target_link_options(925 PRIVATE -mcpu=cortex-m3 -mthumb --specs=nano.specs --specs=nosys.specs -nostartfiles
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F103RCTx_FLASH.ld
    -Wl,--gc-sections -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/925.map -Wl,--print-memory-usage)
set_target_properties(925 PROPERTIES SUFFIX ".elf" LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32F103RCTx_FLASH.ld)
add_custom_command(TARGET 925 POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:925> ${CMAKE_CURRENT_BINARY_DIR}/925.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:925> ${CMAKE_CURRENT_BINARY_DIR}/925.hex
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:925>)
