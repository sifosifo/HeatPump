/* onewire.h - a part of avr-ds18b20 library
 *
 * Copyright (C) 2016 Jacek Wieczorek
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <inttypes.h>
#include <avr/io.h>
#include <util/delay.h>

#define ONEWIRE_ERROR_OK 	0
#define ONEWIRE_ERROR_COMM 	1

uint8_t onewireInit( volatile uint8_t *port, volatile uint8_t *direction, volatile uint8_t *portin, uint8_t mask );
void onewireWrite( volatile uint8_t *port, volatile uint8_t *direction, volatile uint8_t *portin, uint8_t mask, uint8_t data );
uint8_t onewireRead( volatile uint8_t *port, volatile uint8_t *direction, volatile uint8_t *portin, uint8_t mask );

static inline uint8_t onewireWriteBit(volatile uint8_t *port, volatile uint8_t *direction,
                                      volatile uint8_t *portin, uint8_t mask, uint8_t bit) {
    if (bit) {
        *direction |= mask;
        *port &= ~mask;
        _delay_us(8);
        *direction &= ~mask;
        _delay_us(80);
    } else {
        *direction |= mask;
        *port &= ~mask;
        _delay_us(80);
        *direction &= ~mask;
        _delay_us(2);
    }
    return 0;
}

static inline uint8_t onewireReadBit(volatile uint8_t *port, volatile uint8_t *direction,
                                     volatile uint8_t *portin, uint8_t mask) {
    uint8_t result;
    *direction |= mask;
    *port &= ~mask;
    _delay_us(2);
    *direction &= ~mask;
    _delay_us(15);
    result = (*portin & mask) ? 1 : 0;
    _delay_us(65);
    return result;
}

#endif
