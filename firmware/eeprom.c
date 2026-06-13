#include <avr/eeprom.h>
#include <stdint.h>
#include "config.h"
#include "eeprom.h"
#include "relay.h"

uint16_t currentSlot    = 0;
uint32_t sequenceNumber = 0;

/* ---- Forward Declarations ---- */

static uint16_t slotAddr(uint16_t slot);
static uint8_t  readSlot(uint16_t slot, uint8_t *r, uint32_t *t, uint32_t *s);
static void     findLatest(uint8_t *found, uint16_t *slot);

/* ---- Offset Helpers ---- */

static inline uint16_t slotAddr(uint16_t slot) {
    return slot * EEPROM_SLOT_SIZE;
}

/* ---- Fletcher-16 Checksum ---- */

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

/* ---- Slot I/O ---- */

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

/* ---- Slot Selection ---- */

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

/* ---- External references ---- */
extern uint32_t remainingTime;

/* ---- Public API ---- */

void saveState(void) {
    sequenceNumber++;
    if (sequenceNumber == 0) sequenceNumber = 1;

    currentSlot = (currentSlot + 1) % EEPROM_NUM_SLOTS;
    uint16_t a = slotAddr(currentSlot);

    uint16_t cs = makeChecksum(currentRelay, remainingTime, sequenceNumber);

    /* INVALIDATE */
    eeprom_write_byte((uint8_t*)(a+0), 0x00);
    eeprom_write_byte((uint8_t*)(a+1), 0x00);

    /* DATA */
    eeprom_write_byte ((uint8_t*)(a+3), currentRelay);
    eeprom_write_dword((uint32_t*)(a+4), remainingTime);
    eeprom_write_dword((uint32_t*)(a+8), sequenceNumber);
    eeprom_write_word ((uint16_t*)(a+12), cs);

    /* COMMIT (re-validate) */
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

    /* No valid state found — start fresh */
    currentRelay   = 1;
    remainingTime  = CYCLE_TIME_MS;
    sequenceNumber = 0;
    currentSlot    = 0;
    saveState();
}
