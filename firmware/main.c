/*
 * Pool Chlorinator Timer Controller - ATtiny85 V2.3
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
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <stdint.h>
#include "config.h"
#include "timer.h"
#include "relay.h"
#include "eeprom.h"

/* ---- Globals ---- */
uint32_t remainingTime = CYCLE_TIME_MS;
uint32_t lastSaveTime  = 0;
uint32_t lastLoopTime  = 0;

int main(void) {
    wdt_disable();

    DDRB  |= (1 << RELAY1_PIN) | (1 << RELAY2_PIN);
    allRelaysOff();

    initTimer();
    sei();

    /* 8-second startup delay — lets relays and PSU stabilise */
    wdt_enable(WDTO_500MS);
    uint32_t t0 = safeMillis();
    while (timeDiff(safeMillis(), t0) < STARTUP_DELAY_MS) {
        wdt_reset();
    }

    loadState();

    /* Belt-and-suspenders — even with checksum validation in readSlot() */
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

        /* Decrement remaining time */
        if (remainingTime > dt) {
            remainingTime -= dt;
        } else {
            remainingTime = 0;
        }

        /* Relay toggle at end of cycle */
        if (remainingTime == 0) {
            saveState();                        // save pre-switch state
            allRelaysOff();

            /* Dead-time: wait with both relays off */
            uint32_t offT = safeMillis();
            while (timeDiff(safeMillis(), offT) < RELAY_DEADTIME_MS) {
                wdt_reset();
            }

            currentRelay  = (currentRelay == 1) ? 2 : 1;
            remainingTime = CYCLE_TIME_MS;
            setRelays();
            saveState();                        // save post-switch state

            now = safeMillis();
            lastLoopTime = now;
            lastSaveTime = now;
        }

        /* Periodic state save */
        if (timeDiff(now, lastSaveTime) >= SAVE_INTERVAL_MS) {
            saveState();
            lastSaveTime = now;
        }
    }
}
