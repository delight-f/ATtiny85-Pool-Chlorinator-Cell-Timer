#include <avr/io.h>
#include <avr/interrupt.h>
#include "config.h"
#include "relay.h"

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
    /* ~2.5 µs settling delay at 8 MHz */
    for (volatile uint8_t i = 0; i < 20; i++);

    if (currentRelay == 1) {
        PORTB |= (1 << RELAY1_PIN);
    } else {
        PORTB |= (1 << RELAY2_PIN);
    }

    SREG = s;
}
