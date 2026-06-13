/*
 * Pool Chlorinator Timer Controller - ATtiny85 V2.3
 *
 * This file is auto-generated from firmware/*.c + firmware/*.h by `make ino`.
 * Edit the source modules in firmware/ and regenerate rather than editing
 * this file directly.
 *
 * - Alternates between two relays every 12 hours
 * - 5-second dead-time, both relays off during switching
 * - Robust power-loss recovery (remaining-time based)
 * - EEPROM wear leveling (36 slots @ 14 bytes = 504, fits 512)
 * - Invalidate-then-commit EEPROM pattern:
 *   magic cleared → data written → version/magic restored
 * - Checksum + versioned EEPROM state
 * - 32-bit sequence (no wrap concerns for device lifetime)
 * - Watchdog protected
 * - Overflow-safe timing (no arbitrary dt cap)
 *
 * 8 MHz internal RC
 *
 * ---- Wiring ----
 *   PB3  → Relay 1 → SCR 1
 *   PB2  → Relay 2 → SCR 2
 *   VCC  → +5V
 *   GND  → GND
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <stdint.h>

/* ======================== config.h ======================== */

#define RELAY1_PIN PB3
#define RELAY2_PIN PB2

#define CYCLE_TIME_MS      43200000UL
#define STARTUP_DELAY_MS   8000UL
#define RELAY_DEADTIME_MS  5000UL
#define SAVE_INTERVAL_MS   300000UL

#define EEPROM_SLOT_SIZE   14
#define EEPROM_NUM_SLOTS   36
#define EEPROM_MAGIC_1     0xA5
#define EEPROM_MAGIC_2     0x5A
#define EEPROM_VERSION     1

/* ======================== globals.h ======================== */

/* Declared here so all functions below can see them.
 * Defined at the bottom before main(). */
extern uint32_t remainingTime;

/* ======================== timer.c ======================== */

volatile uint32_t millisCounter = 0;

ISR(TIMER0_COMPA_vect) {
    millisCounter++;
}

void initTimer(void) {
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS01) | (1 << CS00);
    OCR0A  = 124;
    TIMSK |= (1 << OCIE0A);
}

uint32_t safeMillis(void) {
    uint32_t m;
    uint8_t s = SREG;
    cli();
    m = millisCounter;
    SREG = s;
    return m;
}

static inline uint32_t timeDiff(uint32_t now, uint32_t prev) {
    return now - prev;
}

/* ======================== relay.c ======================== */

uint8_t currentRelay = 1;

void allRelaysOff(void) {
    PORTB &= ~((1 << RELAY1_PIN) | (1 << RELAY2_PIN));
}

void setRelays(void) {
    if (currentRelay < 1 || currentRelay > 2) {
        currentRelay = 1;
    }

    uint8_t s = SREG;
    cli();

    allRelaysOff();
    for (volatile uint8_t i = 0; i < 20; i++);

    if (currentRelay == 1) {
        PORTB |= (1 << RELAY1_PIN);
    } else {
        PORTB |= (1 << RELAY2_PIN);
    }

    SREG = s;
}

/* ======================== eeprom.c ======================== */

uint16_t currentSlot    = 0;
uint32_t sequenceNumber = 0;

static inline uint16_t slotAddr(uint16_t slot) {
    return slot * EEPROM_SLOT_SIZE;
}

uint16_t makeChecksum(uint8_t r, uint32_t t, uint32_t s) {
    uint16_t a = 0, b = 0;
    uint8_t d[9] = {
        r,
        (uint8_t)(t >> 24), (uint8_t)(t >> 16),
        (uint8_t)(t >> 8),  (uint8_t)t,
        (uint8_t)(s >> 24), (uint8_t)(s >> 16),
        (uint8_t)(s >> 8),  (uint8_t)s
    };
    for (uint8_t i = 0; i < 9; i++) {
        a += d[i]; while (a >= 255) a -= 255;
        b += a;    while (b >= 255) b -= 255;
    }
    return (b << 8) | a;
}

static uint8_t readSlot(uint16_t slot, uint8_t *r, uint32_t *t, uint32_t *s) {
    uint16_t a = slotAddr(slot);

    if (eeprom_read_byte((uint8_t*)(a+0)) != EEPROM_MAGIC_1) return 0;
    if (eeprom_read_byte((uint8_t*)(a+1)) != EEPROM_MAGIC_2) return 0;
    if (eeprom_read_byte((uint8_t*)(a+2)) != EEPROM_VERSION) return 0;

    *r = eeprom_read_byte((uint8_t*)(a+3));
    *t = eeprom_read_dword((uint32_t*)(a+4));
    *s = eeprom_read_dword((uint32_t*)(a+8));

    if (*r < 1 || *r > 2) return 0;
    if (*t > CYCLE_TIME_MS) return 0;

    uint16_t cs = eeprom_read_word((uint16_t*)(a+12));
    if (cs != makeChecksum(*r, *t, *s)) return 0;

    return 1;
}

static void findLatest(uint8_t *found, uint16_t *slot) {
    *found = 0;
    uint32_t bestSeq = 0;

    for (uint16_t i = 0; i < EEPROM_NUM_SLOTS; i++) {
        uint8_t r; uint32_t t; uint32_t s;
        if (readSlot(i, &r, &t, &s)) {
            if (!*found || (int32_t)(s - bestSeq) > 0) {
                bestSeq = s;
                *slot = i;
                *found = 1;
            }
        }
    }
}

void saveState(void) {
    sequenceNumber++;
    if (sequenceNumber == 0) sequenceNumber = 1;

    currentSlot = (currentSlot + 1) % EEPROM_NUM_SLOTS;
    uint16_t a = slotAddr(currentSlot);

    uint16_t cs = makeChecksum(currentRelay, remainingTime, sequenceNumber);

    eeprom_write_byte((uint8_t*)(a+0), 0x00);
    eeprom_write_byte((uint8_t*)(a+1), 0x00);

    eeprom_write_byte ((uint8_t*)(a+3), currentRelay);
    eeprom_write_dword((uint32_t*)(a+4), remainingTime);
    eeprom_write_dword((uint32_t*)(a+8), sequenceNumber);
    eeprom_write_word ((uint16_t*)(a+12), cs);

    eeprom_write_byte((uint8_t*)(a+2), EEPROM_VERSION);
    eeprom_write_byte((uint8_t*)(a+1), EEPROM_MAGIC_2);
    eeprom_write_byte((uint8_t*)(a+0), EEPROM_MAGIC_1);
}

void loadState(void) {
    uint8_t found;
    uint16_t slot;

    findLatest(&found, &slot);

    if (found) {
        uint8_t r; uint32_t t; uint32_t s;
        if (readSlot(slot, &r, &t, &s)) {
            currentRelay   = r;
            remainingTime  = (t == 0) ? CYCLE_TIME_MS : t;
            sequenceNumber = s;
            currentSlot    = slot;
            return;
        }
    }

    currentRelay   = 1;
    remainingTime  = CYCLE_TIME_MS;
    sequenceNumber = 0;
    currentSlot    = 0;
    saveState();
}

/* ======================== main.c ======================== */

uint32_t remainingTime = CYCLE_TIME_MS;
uint32_t lastSaveTime  = 0;
uint32_t lastLoopTime  = 0;

int main(void) {
    wdt_disable();

    DDRB  |= (1 << RELAY1_PIN) | (1 << RELAY2_PIN);
    allRelaysOff();

    initTimer();
    sei();

    wdt_enable(WDTO_500MS);
    uint32_t t0 = safeMillis();
    while (timeDiff(safeMillis(), t0) < STARTUP_DELAY_MS) {
        wdt_reset();
    }

    loadState();

    if (remainingTime > CYCLE_TIME_MS) remainingTime = CYCLE_TIME_MS;

    setRelays();

    lastSaveTime = safeMillis();
    lastLoopTime = safeMillis();

    wdt_enable(WDTO_2S);

    while (1) {
        wdt_reset();

        uint32_t now = safeMillis();
        uint32_t dt  = now - lastLoopTime;
        lastLoopTime = now;

        if (remainingTime > dt) {
            remainingTime -= dt;
        } else {
            remainingTime = 0;
        }

        if (remainingTime == 0) {
            saveState();
            allRelaysOff();

            uint32_t offT = safeMillis();
            while (timeDiff(safeMillis(), offT) < RELAY_DEADTIME_MS) {
                wdt_reset();
            }

            currentRelay  = (currentRelay == 1) ? 2 : 1;
            remainingTime = CYCLE_TIME_MS;
            setRelays();
            saveState();

            now = safeMillis();
            lastLoopTime = now;
            lastSaveTime = now;
        }

        if (timeDiff(now, lastSaveTime) >= SAVE_INTERVAL_MS) {
            saveState();
            lastSaveTime = now;
        }
    }
}