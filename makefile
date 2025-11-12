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
SRC = $(wildcard *.c ds1820/*.c)
OBJ = $(SRC:.c=.o)

CC = avr-gcc
CFLAGS = -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -I./include -I.
LDFLAGS = -mmcu=$(MCU) -Wl,-u,vfprintf -lprintf_min

AVRDUDE = avrdude

# === RULES ===
all: $(TARGET).hex

$(TARGET).hex: $(TARGET).elf
	$(Q)avr-objcopy -O ihex -R .eeprom $< $@

$(TARGET).elf: $(OBJ) libcan.a
	$(Q)$(CC) $(LDFLAGS) -o $@ $(OBJ) libcan.a

%.o: %.c
	$(Q)$(CC) $(CFLAGS) -c $< -o $@

flash: $(TARGET).hex
	$(AVRDUDE) -F -V -c arduino -p $(MCU) -P /dev/ttyACM0 -b 115200 -U flash:w:$<

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).hex

size: $(TARGET).elf
	avr-size --format=avr --mcu=$(MCU) $<

.PHONY: all clean flash size
