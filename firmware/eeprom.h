#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>

/* The current write slot index (0 .. EEPROM_NUM_SLOTS-1) */
extern uint16_t currentSlot;

/* The monotonically increasing save sequence number */
extern uint32_t sequenceNumber;

/**
 * Compute a Fletcher-16 checksum over the state payload.
 * Used to detect corruption in saved EEPROM slots.
 */
uint16_t makeChecksum(uint8_t r, uint32_t t, uint32_t s);

/**
 * Restore state from the most recent valid EEPROM slot.
 * If no valid slot is found, initialise to defaults and save.
 */
void loadState(void);

/**
 * Atomically write current state to the next EEPROM slot.
 * Invalidates the slot first, writes data, then commits.
 */
void saveState(void);

#endif /* EEPROM_H */
