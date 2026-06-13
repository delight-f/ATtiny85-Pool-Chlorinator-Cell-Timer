# ATTiny85 Pool Chlorinator Controller

A drop-in replacement microcontroller for ageing **Compuchlor** pool chlorinator units. Alternates two relays every 12 hours to reverse cell polarity, reducing calcium buildup on the electrolytic plates.

---

## Background

Many older Compuchlor pool chlorinators used a microcontroller that is no longer available or costs more to replace than the unit is worth. This project provides a **direct functional replacement** using an ATTiny85 — a cheap, widely available, through-hole MCU.

The original Compuchlor design drives two SCRs via relays to reverse the DC polarity across the chlorinator cell. Polarity reversal prevents hard calcium scale from bonding to the plates, which is essential for long cell life.

---

## How It Works

```
┌──────────────────────────────────────────────────────────────┐
│                       12-Hour Cycle                         │
│                                                              │
│  Relay 1 ON ──────────────────────────────────┤   RELAY 1   │
│  Relay 2 OFF                                │   RELAY 2   │
│                                              │              │
│                  5s dead-time                │   5s gap     │
│                  (both off)                  │  (both off)  │
│                                              │              │
│  Relay 1 OFF                    ┌────────────┤   RELAY 1   │
│  Relay 2 ON  ───────────────────┤            │   RELAY 2   │
│                                 │            │              │
│  └────────── 12 hours ──────────┘  └──── 12 hours ─────────┘│
│                                                              │
│              Alternating polarity every 12h                  │
└──────────────────────────────────────────────────────────────┘
```

- **Relay 1** → SCR 1 → Cell polarity A
- **Relay 2** → SCR 2 → Cell polarity B (reversed)
- **5-second dead-time** between transitions, both relays off, preventing SCR shoot-through
- Timer starts immediately on power-up (after an 8-second settling delay)

---

## Features

| Feature | Detail |
|---|---|
| **Cycle time** | 12 hours per polarity (24-hour full cycle) |
| **Dead-time** | 5 seconds, both relays off during switch |
| **Power-loss recovery** | State saved to EEPROM every 5 minutes + on every relay transition |
| **EEPROM wear leveling** | 36 rotating slots, extends life to ~18M+ writes (vs ~100K for a fixed slot) |
| **Atomic commit** | Invalidate-then-write pattern — power failure during save never corrupts state |
| **Checksum protection** | Fletcher-16 over all saved state |
| **Watchdog timer** | 2-second WDT prevents lockups |
| **Overflow-safe** | All time arithmetic uses unsigned modular wrap |

---

## Hardware

### Pinout

| ATTiny85 Pin | Function |
|---|---|
| PB3 (pin 2) | Relay 1 — drives SCR 1 |
| PB2 (pin 7) | Relay 2 — drives SCR 2 |
| VCC (pin 8) | +5V DC |
| GND (pin 4) | Ground |

The remaining pins (PB0, PB1, PB4, PB5) are unused. PB5 is the RESET pin — leave it pulled high per the ATtiny85 datasheet.

### Bill of Materials

| Component | Qty | Notes |
|---|---|---|
| ATTiny85-20PU | 1 | DIP-8, programmed at 8 MHz internal RC |
| 5V relay module (x2) or discrete relays + driver transistors | 2 | Driven by PB3/PB2, active-high |
| 100 nF decoupling cap | 1 | Across VCC/GND near the MCU |
| 10 kΩ pull-up | 1 | Between PB5 (RESET) and VCC |
| Programming header | 1 | 6-pin ISP for initial flash |

### Relay Wiring to SCRs

```
      ┌──────────┐
      │  ATTiny85 │
      │           │
      │  PB3 ─────┼──→ Relay 1 coil ─── SCR 1 gate
      │  PB2 ─────┼──→ Relay 2 coil ─── SCR 2 gate
      └──────────┘
                    ╔══════════════════════════════╗
                    ║  Relay 1 NO ──→ SCR 1       ║
                    ║  Relay 2 NO ──→ SCR 2       ║
                    ║  SCRs drive cell in          ║
                    ║  alternating polarity        ║
                    ╚══════════════════════════════╝
```

---

## EEPROM Reliability

This is the most critical design detail for an embedded controller expected to run for years.

### Wear Leveling

The ATTiny85 has **512 bytes** of EEPROM rated for ~100,000 writes. Without wear leveling, writing every 5 minutes would exhaust the EEPROM in about **347 days**.

This firmware rotates through **36 slots** (14 bytes each, total 504 bytes). With a save every 5 minutes:

| Scenario | Lifespan |
|---|---|
| **Fixed single slot** | ~347 days |
| **36-slot wear leveling** | ~12,500 days (~34 years) |

### Atomic Commit Protocol

Each save follows a strict sequence:

```
1. Invalidate:   magic = 0x00, 0x00
2. Write data:   relay state, remaining time, sequence, checksum
3. Commit:       version → magic2 → magic1 (written in that order)
```

- If power fails during **step 2**: magic is 0x00 → slot rejected on boot
- If power fails during **step 3**: partial magic → slot rejected on boot
- The **final byte** written is magic1 — once it reads 0xA5, the entire slot is guaranteed consistent
- A Fletcher-16 checksum provides a second line of defense

---

## Project Structure

```
chlorinator_controller/
├── README.md                          # ← you are here
├── LICENSE                            # MIT
├── Makefile                           # avr-gcc build + avrdude flash + ino generation
├── chlorinator_controller.ino         # Arduino IDE upload (auto-generated)
└── src/
    ├── config.h                       # Pin defines, timing constants, EEPROM layout
    ├── timer.c / timer.h              # Timer0 1ms tick, safeMillis(), timeDiff()
    ├── relay.c / relay.h              # Relay control (off, set with settling delay)
    ├── eeprom.c / eeprom.h            # EEPROM wear-leveling, atomic save/load, Fletcher-16
    └── main.c                         # Entry point, startup, main timing loop
```

There are **two ways** to work with this code, depending on your comfort level:

| Method | For | How |
|---|---|---|
| **src/ + Makefile** | Developers | `make all` → `.hex`, `make flash` → upload |
| **.ino file** | Beginners / Arduino IDE users | Open `chlorinator_controller.ino`, select board, click Upload |

The `.ino` file is **auto-generated** from the `src/` modules by running `make ino`. If you edit `src/`, regenerate it to keep them in sync.

---

## Build & Flash

### Option 1 — avr-gcc (command line, recommended for developers)

**Prerequisites:** `avr-gcc`, `avr-binutils`, `avr-libc`, `avrdude`

```bash
# Build
make

# Set fuses (one-time per chip)
make fuses

# Flash
make flash
```

This produces `chlorinator.hex` and shows flash/RAM usage.

### Option 2 — Arduino IDE (point-and-click, great for beginners)

1. Open `chlorinator_controller.ino` in the Arduino IDE
2. Install **ATTinyCore** or **David Mellis's ATtiny board support** via Boards Manager
3. Select board: **ATtiny85 @ 8 MHz (internal)**
4. Select programmer: **USBasp** (or your ISP)
5. Click **Upload**

### Fuse Settings

The ATTiny85 must be set to run from the internal 8 MHz RC oscillator. This is a **one-time** operation per chip:

| Fuse | Value |
|---|---|
| `lfuse` | `0xE2` |
| `hfuse` | `0xDF` |
| `efuse` | `0xFF` |

```bash
make fuses   # or the avrdude command below
avrdude -c usbasp -p t85 -U lfuse:w:0xE2:m -U hfuse:w:0xDF:m -U efuse:w:0xFF:m
```

> **Important:** The code relies on an 8 MHz clock for accurate 1ms timer ticks. Do not change the clock source without adjusting `OCR0A` and the timing constants.

---

## Firmware Architecture

```
┌──────────────────────────────────────────────┐
│  Timer0 COMPA @ 1ms                          │
│  └→ millisCounter++                          │
├──────────────────────────────────────────────┤
│  safeMillis()           ← atomic uint32 read  │
│  timeDiff(now, prev)    ← wrap-safe subtract │
├──────────────────────────────────────────────┤
│  loadState()                                  │
│  └→ findLatest() → readSlot() → Fletcher-16  │
│       validates magic, version, checksum     │
│       picks slot with highest sequence #     │
├──────────────────────────────────────────────┤
│  saveState()                                  │
│  └→ invalidate → write → commit (byte-by-byte)│
├──────────────────────────────────────────────┤
│  Main loop                                    │
│  ├─ Decrement remainingTime by wall-clock dt │
│  ├─ If remainingTime == 0:                    │
│  │   save pre-switch → dead-time → toggle    │
│  │   relay → save post-switch                │
│  └─ Periodic save every 5 min                │
└──────────────────────────────────────────────┘
```

### Key Design Decisions

- **No `delay()` or `delayMicroseconds()`** — these block the WDT and prevent interrupt processing. All waits are WDT-reset loops.
- **No dynamic memory** — everything is stack/global. ATtiny85 has only 512 bytes of SRAM.
- **`int main()` instead of `setup()`/`loop()`** — compiled as plain C, no Arduino overhead.
- **Volatile relay settler** — a short inline loop gives the relay contacts time to settle before the SCR fires.

---

## License

This project is open-source hardware/software. Use it, modify it, and share it freely.

---

## Disclaimer

This firmware controls mains-voltage pool equipment. Test thoroughly on a workbench before connecting to your chlorinator. The author assumes no liability for damage to equipment or injury resulting from use of this project.
