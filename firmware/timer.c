#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer.h"

volatile uint32_t millisCounter = 0;

ISR(TIMER0_COMPA_vect) {
    millisCounter++;
}

void initTimer(void) {
    TCCR0A = (1 << WGM01);                    // CTC mode
    TCCR0B = (1 << CS01) | (1 << CS00);       // clk/64 prescaler
    OCR0A  = 124;                              // 1 ms @ 8 MHz
    TIMSK |= (1 << OCIE0A);                    // enable compare A interrupt
}

uint32_t safeMillis(void) {
    uint32_t m;
    uint8_t s = SREG;
    cli();
    m = millisCounter;
    SREG = s;
    return m;
}
