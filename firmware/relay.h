#ifndef RELAY_H
#define RELAY_H

#include <stdint.h>

/* The currently active relay (1 or 2) */
extern uint8_t currentRelay;

/* Turn both relays off */
void allRelaysOff(void);

/**
 * Energise the relay corresponding to currentRelay.
 * A brief settling delay (~2.5 µs) is inserted after de-energising
 * both relays before the selected relay is turned on.
 * Interrupts are masked during the transition.
 */
void setRelays(void);

#endif /* RELAY_H */
