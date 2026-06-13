#ifndef CONFIG_H
#define CONFIG_H

/* ---- Pin Definitions ---- */
#define RELAY1_PIN PB3
#define RELAY2_PIN PB2

/* ---- Timing ---- */
#define CYCLE_TIME_MS      43200000UL   // 12 hours
#define STARTUP_DELAY_MS   8000UL
#define RELAY_DEADTIME_MS  5000UL
#define SAVE_INTERVAL_MS   300000UL     // 5 minutes

/* ---- EEPROM Layout ---- */
/* ATtiny85 EEPROM = 512 B.  36 × 14 = 504 B */
#define EEPROM_SLOT_SIZE   14
#define EEPROM_NUM_SLOTS   36
#define EEPROM_MAGIC_1     0xA5
#define EEPROM_MAGIC_2     0x5A
#define EEPROM_VERSION     1

/*
 * Slot (14 bytes):
 * [0]  magic1   ← written LAST (commit)
 * [1]  magic2
 * [2]  version
 * [3]  relay (1|2)
 * [4]  remainingTime (uint32)
 * [8]  sequence (uint32)
 * [12] checksum (uint16)
 *
 * Commit sequence:
 *   1. magic1 = 0x00, magic2 = 0x00   (invalidate)
 *   2. write data + checksum
 *   3. write version
 *   4. magic2 = 0x5A, magic1 = 0xA5   (re-validate)
 *
 * If power fails during step 2, magic is 0x00 → slot rejected.
 * During steps 3-4, partial magic → slot rejected.
 * Final byte commit ensures atomic visibility.
 */

#endif /* CONFIG_H */
