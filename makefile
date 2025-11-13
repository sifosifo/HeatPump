# === SILENT BUILD ===
ifeq ($(V),1)
    Q =
else
   Q = @
   $(info === Building in silent mode. Use `make V=1` for verbose ===)
endif

# === CONFIGURATION ===
MCU = atmega328p
F_CPU = 16000000UL
TARGET = firmware
BUILD_DIR = build

# Source files
SRC = $(wildcard src/*.c)
OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)
DEP = $(OBJ:.o=.d)

CC = avr-gcc
CFLAGS = -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -Iinclude -MMD -MP
LDFLAGS = -mmcu=$(MCU) -Wl,-u,vfprintf -lprintf_min

AVRDUDE = avrdude

# === RULES ===
all: $(BUILD_DIR)/$(TARGET).hex

$(BUILD_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf
	$(Q)avr-objcopy -O ihex -R .eeprom $< $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJ) lib/libcan.a | $(BUILD_DIR)
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJ) lib/libcan.a

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	$(Q)mkdir -p $@

flash: $(BUILD_DIR)/$(TARGET).hex
	$(Q)$(AVRDUDE) -F -V -c arduino -p $(MCU) -P /dev/ttyACM0 -b 115200 -U flash:w:$<

clean:
	$(Q)rm -rf $(BUILD_DIR)

size: $(BUILD_DIR)/$(TARGET).elf
	$(Q)avr-size --format=avr --mcu=$(MCU) $<

# Include dependency files
-include $(DEP)

.PHONY: all clean flash size
