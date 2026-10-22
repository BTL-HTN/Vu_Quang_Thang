
TARGET = main

CC      = arm-none-eabi-gcc
AS      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

STFLASH = st-flash

GDB     = gdb-multiarch

SRC_DIRS = User/src \
		  Startup \
		  Lib/stm32f10x_lib/STM32F10x_StdPeriph_Driver/src\
		  Lib/stm32f10x_lib/CMSIS/CM3/CoreSupport\
		  Lib/stm32f10x_lib/CMSIS/CM3/DeviceSupport/ST/STM32F10x\
		  Lib/freertos\
		  Lib/freertos/portable/GCC/ARM_CM3\
		  MyLib/src\
		  Driver/MFRC522\
		  Driver/KEYPAD\
		  Driver/BUZZER
		  

SRC_FILES +=$(foreach SRC_DIRS,$(SRC_DIRS),$(wildcard $(SRC_DIRS)/*.c))
HEAP_DIR = Lib/freertos/portable/MemMang

SRC_FILES += $(HEAP_DIR)/heap_4.c

INC_DIRS = User/inc \
			  Startup \
			  Lib/stm32f10x_lib/STM32F10x_StdPeriph_Driver/inc\
			  Lib/stm32f10x_lib/CMSIS/CM3/DeviceSupport/ST/STM32F10x\
			  Lib/stm32f10x_lib/CMSIS/CM3/CoreSupport\
			  Lib/freertos\
		 	  Lib/freertos/portable/GCC/ARM_CM3\
			  Lib/freertos/include\
			  Lib/stm32f10x_lib/STM32F10x_StdPeriph_Driver\
			  MyLib/inc\
			  Driver/MFRC522\
			  Driver/KEYPAD\
			  Driver/BUZZER

INC_FILES +=$(foreach INC_DIRS,$(INC_DIRS),$(wildcard $(INC_DIRS)/*.h))
INC_DIR_OPT = $(foreach INC_DIRS,$(INC_DIRS),-I$(INC_DIRS))

PATH_OUTPUT = Output

LD_File = Linker/linker.ld

MCU = cortex-m3

CHIP=cortex-m3

CCFLAGS = -c -mcpu=$(CHIP) -mthumb -std=gnu11 -Os -Wall \
          -ffunction-sections -fdata-sections \
          $(INC_DIR_OPT) -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER

vpath %.c $(SRC_DIRS) $(HEAP_DIR)

OBJ_FILE := $(notdir $(SRC_FILES))
OBJ_FILE := $(subst .c,.o,$(OBJ_FILE))
PATH_OBJ := $(foreach OBJ_FILE,$(OBJ_FILE),$(PATH_OUTPUT)/$(OBJ_FILE))
LDFLAGS = -mcpu=$(CHIP) -mthumb -nostdlib -T $(LD_File) \
          -Wl,-Map=$(PATH_OUTPUT)/$(TARGET).map -Wl,--gc-sections
LIBS    = -Wl,--start-group -lc_nano -lgcc -lnosys -Wl,--end-group

build: $(PATH_OUTPUT)/$(TARGET).bin

$(PATH_OUTPUT)/%.o: %.c
	@mkdir -p $(PATH_OUTPUT)
	$(CC) $(CCFLAGS) $< -o $@

$(PATH_OUTPUT)/$(TARGET).elf: $(PATH_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(PATH_OUTPUT)/$(TARGET).bin: $(PATH_OUTPUT)/$(TARGET).elf
	$(OBJCOPY) -O binary "$^" "$@"

flash: Output/$(TARGET).bin
	$(STFLASH) write Output/$(TARGET).bin 0x08000000

erase:
	$(STFLASH) erase

clean:
	rm -f Output/*
print-%:
	@echo $($(subst print-,,$@))