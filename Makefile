MCU   = attiny85
F_CPU = 8000000UL

CC      = avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size
CFLAGS  = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall -Wextra -std=c99

SRC  = firmware/main.c firmware/timer.c firmware/relay.c firmware/eeprom.c
OBJ  = $(SRC:.c=.o)
TARGET = chlorinator

# ---------------------------------------------------------------------------
# Default target: build .elf + .hex + show size
# ---------------------------------------------------------------------------

.PHONY: all clean flash fuses size

all: $(TARGET).elf $(TARGET).hex size

$(TARGET).elf: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

size: $(TARGET).elf
	$(SIZE) --mcu=$(MCU) --format=avr $<

# ---------------------------------------------------------------------------
# Programming (adjust programmer type as needed)
# ---------------------------------------------------------------------------

flash: $(TARGET).hex
	avrdude -c usbasp -p t85 -U flash:w:$<

# Fuses for internal 8 MHz RC:
#   lfuse = 0xE2, hfuse = 0xDF, efuse = 0xFF
fuses:
	avrdude -c usbasp -p t85 \
		-U lfuse:w:0xE2:m \
		-U hfuse:w:0xDF:m \
		-U efuse:w:0xFF:m

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Regenerate the Arduino-IDE-ready .ino from src/ modules
# ---------------------------------------------------------------------------

INO_FILE = chlorinator_controller.ino

ino: $(INO_FILE)

$(INO_FILE): firmware/config.h firmware/timer.c firmware/relay.c firmware/eeprom.c firmware/main.c
	@echo "Regenerating $(INO_FILE) from firmware/..."
	@printf '%s\n' \
	  '/*' \
	  ' * Pool Chlorinator Timer Controller - ATtiny85 V2.3' \
	  ' *' \
	  ' * This file is auto-generated from firmware/*.c + firmware/*.h by `make ino`.' \
	  ' * Edit the source modules in firmware/ and regenerate rather than editing' \
	  ' * this file directly.' \
	  ' *' \
	  ' * - Alternates between two relays every 12 hours' \
	  ' * - 5-second dead-time, both relays off during switching' \
	  ' * - Robust power-loss recovery (remaining-time based)' \
	  ' * - EEPROM wear leveling (36 slots @ 14 bytes = 504, fits 512)' \
	  ' * - Invalidate-then-commit EEPROM pattern:' \
	  ' *   magic cleared $$\rightarrow$$ data written $$\rightarrow$$ version/magic restored' \
	  ' * - Checksum + versioned EEPROM state' \
	  ' * - 32-bit sequence (no wrap concerns for device lifetime)' \
	  ' * - Watchdog protected' \
	  ' * - Overflow-safe timing (no arbitrary dt cap)' \
	  ' *' \
	  ' * 8 MHz internal RC' \
	  ' *' \
	  ' * ---- Wiring ----' \
	  ' *   PB3  $$\rightarrow$$ Relay 1 $$\rightarrow$$ SCR 1' \
	  ' *   PB2  $$\rightarrow$$ Relay 2 $$\rightarrow$$ SCR 2' \
	  ' *   VCC  $$\rightarrow$$ +5V' \
	  ' *   GND  $$\rightarrow$$ GND' \
	  ' */' \
	  '' \
	  '#include <avr/io.h>' \
	  '#include <avr/interrupt.h>' \
	  '#include <avr/eeprom.h>' \
	  '#include <avr/wdt.h>' \
	  '#include <stdint.h>' \
	  '' > $(INO_FILE).tmp
	@for f in firmware/config.h firmware/timer.c firmware/relay.c firmware/eeprom.c firmware/main.c; do \
	  echo "/* ======================== $$(basename $$f) ======================== */" >> $(INO_FILE).tmp; \
	  sed '1,/^\/\* =/d' $$f >> $(INO_FILE).tmp; \
	  echo '' >> $(INO_FILE).tmp; \
	done
	@mv $(INO_FILE).tmp $(INO_FILE)
	@echo "Done: $(INO_FILE)"

# ---------------------------------------------------------------------------
# Clean
# ---------------------------------------------------------------------------

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).hex
