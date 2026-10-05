PREFIX ?= arm-none-eabi-
CC = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE = $(PREFIX)size
BUILD ?= build-rct6
DEMO ?= 0
HAL = Drivers/STM32F1xx_HAL_Driver
DEVICE = Drivers/CMSIS/Device/ST/STM32F1xx
SOURCES = Core/Src/main.c ../stm32/batch_link.c $(DEVICE)/Source/Templates/system_stm32f1xx.c
SOURCES += $(addprefix $(HAL)/Src/stm32f1xx_hal, .c _cortex.c _dma.c _gpio.c _spi.c _rcc.c _rcc_ex.c _flash.c _flash_ex.c _pwr.c)
STARTUP = $(DEVICE)/Source/Templates/gcc/startup_stm32f103xe.s
OBJECTS = $(addprefix $(BUILD)/,$(notdir $(SOURCES:.c=.o))) $(BUILD)/startup.o
vpath %.c $(sort $(dir $(SOURCES)))
CFLAGS = -mcpu=cortex-m3 -mthumb -Os -g3 -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
CPPFLAGS = -DBATCH_DEMO=$(DEMO) -DUSE_HAL_DRIVER -DSTM32F103xE -ICore/Inc -I../stm32 -I$(HAL)/Inc -I$(DEVICE)/Include -IDrivers/CMSIS/Include
LDFLAGS = -mcpu=cortex-m3 -mthumb --specs=nano.specs --specs=nosys.specs -nostartfiles -TSTM32F103RCTx_FLASH.ld -Wl,--gc-sections -Wl,-Map=$(BUILD)/925.map -Wl,--print-memory-usage
.PHONY: all
all: $(BUILD)/925.elf $(BUILD)/925.bin $(BUILD)/925.hex
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: %.c Makefile | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/startup.o: $(STARTUP) Makefile | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/925.elf: $(OBJECTS) STM32F103RCTx_FLASH.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	$(SIZE) $@
$(BUILD)/925.bin: $(BUILD)/925.elf
	$(OBJCOPY) -O binary $< $@
$(BUILD)/925.hex: $(BUILD)/925.elf
	$(OBJCOPY) -O ihex $< $@
-include $(OBJECTS:.o=.d)
