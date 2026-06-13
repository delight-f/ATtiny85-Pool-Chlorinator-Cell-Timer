#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Volatile counter incremented every 1 ms by Timer0 COMPA ISR */
extern volatile uint32_t millisCounter;

/* Initialise Timer0 for a 1 ms tick (expects 8 MHz clock) */
void initTimer(void);

/* Atomically read the millisecond counter */
uint32_t safeMillis(void);

/**
 * Compute elapsed time between two safeMillis() calls.
 * Safe across uint32_t wrap (~49.7 days) via unsigned modular arithmetic.
 */
static inline uint32_t timeDiff(uint32_t now, uint32_t prev) {
    return now - prev;
}

#endif /* TIMER_H */
