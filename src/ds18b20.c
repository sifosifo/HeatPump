/*
** Dallas DS18B20 Temperature Sensor Library (Atmel AVR)
**
** This is a library to access the Dallas DS18B20 temperature sensor.
** The functions contained within this library implement the Dallas 1-Wire protocol.
*/
/*
** F_CPU should be defined by source code using this library
** If it is not defined, it will be defined with a default value of 1000000UL
** in util/delay.h
*/
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include "ds18b20.h"

#define FALSE 0
#define TRUE  1

/* ROM commands */
#define SEARCH_ROM      0xf0
#define READ_ROM        0x33
#define MATCH_ROM       0x55
#define SKIP_ROM        0xcc
#define ALARM_SEARCH    0xec

/* function commands */
#define CONVERT_T               0x44
#define WRITE_SCRATCHPAD        0x4e
#define READ_SCRATCHPAD         0xbe
#define COPY_SCRATCHPAD         0x48
#define RECALL_E2               0xb8
#define READ_POWER_SUPPLY       0xb4


#define CRC_MASK                0xff00000000000000L
#define SERIAL_MASK             0x00ffffffffffff00L
#define FAMILY_MASK             0x00000000000000ffL


/* registers */
#define TEMPLSB 0
#define TEMPMSB 1
#define THIGH   2
#define TLOW    3
#define CONFIG  4
// reserved     5
// reserved     6
// reserved     7
#define CRC     8


/*
** nominally private routines
*/

// Dallas/Maxim CRC8 for 1-Wire (polynomial 0x8C, reversed)
static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        for (uint8_t j = 0; j < 8; ++j) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

uint8_t _reset(ds18b20_t *p)
{
/* return TRUE if DS18B20 device detected, FALSE if not detected */
    ds_port_t port;
    uint8_t   pin;
    uint8_t   plow;
    uint8_t   phigh;
    uint8_t   dq;

    port  = p->port;
    pin   = p->pin;
    plow  = p->plow;
    phigh = p->phigh;
    dq = FALSE;

    /*
    ** pull bus low for Trstl (480us)
    ** then release and wait a bit before sampling
    */
    switch(port)
    {
#ifdef PORTA
        case DS_PORT_A:
            DDRA = DDRA | phigh;
            PORTA = PORTA & plow;
            _delay_us(Trstl);

            DDRA = DDRA & plow;
            PORTA = PORTA | phigh;  // Enable internal pull-up
            _delay_us(Trstwait);
            dq = (PINA & phigh) >> pin;
            break;
#endif

#ifdef PORTB
        case DS_PORT_B:
            DDRB = DDRB | phigh;
            PORTB = PORTB & plow;
            _delay_us(Trstl);

            DDRB = DDRB & plow;
            PORTB = PORTB | phigh;  // Enable internal pull-up
            _delay_us(Trstwait);
            dq = (PINB & phigh) >> pin;
            break;
#endif

#ifdef PORTC
        case DS_PORT_C:
            DDRC = DDRC | phigh;
            PORTC = PORTC & plow;
            _delay_us(Trstl);

            DDRC = DDRC & plow;
            PORTC = PORTC | phigh;  // Enable internal pull-up
            _delay_us(Trstwait);
            dq = (PINC & phigh) >> pin;
            break;
#endif

#ifdef PORTD
        case DS_PORT_D:
            DDRD = DDRD | phigh;
            PORTD = PORTD & plow;
            _delay_us(Trstl);

            DDRD = DDRD & plow;
            PORTD = PORTD | phigh;  // Enable internal pull-up
            _delay_us(Trstwait);
            dq = (PIND & phigh) >> pin;
            break;
#endif

        default:
            return EBADR;
            break;
    }

    _delay_us(Trsth - Trstwait);

    /* DQ=LOW  <-> present (return TRUE) */
    /* DQ=HIGH <-> not present (return FALSE) */
    return (dq ? FALSE : TRUE);
}


uint8_t _read_dq(ds18b20_t *p)
{
/* returns the DQ level - this fn allows other functions to abstract the reading */
    ds_port_t port;
    uint8_t   pin;
    uint8_t   plow;
    uint8_t   phigh;
    uint8_t   dq;

    port  = p->port;
    pin   = p->pin;
    plow = p->plow;
    phigh = p->phigh;
    dq = 0;

    switch(port)
    {
#ifdef PORTA
        case DS_PORT_A:
            DDRA = DDRA & plow;
            PORTA = PORTA | phigh;  // Enable internal pull-up
            dq = (PINA & phigh) >> pin;
            break;
#endif

#ifdef PORTB
        case DS_PORT_B:
            DDRB = DDRB & plow;
            PORTB = PORTB | phigh;  // Enable internal pull-up
            dq = (PINB & phigh) >> pin;
            break;
#endif

#ifdef PORTC
        case DS_PORT_C:
            DDRC = DDRC & plow;
            PORTC = PORTC | phigh;  // Enable internal pull-up
            dq = (PINC & phigh) >> pin;
            break;
#endif

#ifdef PORTD
        case DS_PORT_D:
            DDRD = DDRD & plow;
            PORTD = PORTD | phigh;  // Enable internal pull-up
            dq = (PIND & phigh) >> pin;
            break;
#endif

        default:
            break;
    }

    return dq;
}


uint8_t _write_byte(ds18b20_t *p, uint8_t data)
{
/* bytes are written LSB first */
    ds_port_t port;
    uint8_t   plow;
    uint8_t   phigh;

    uint8_t n;
    uint8_t ch;
    uint8_t bit;

    port  = p->port;
    plow = p->plow;
    phigh = p->phigh;

    ch = data;
    for (n = 0; n < 8; n++)
    {
        bit = ch % 2;

        if (bit == 0)
        {
            /*
            ** write 0
            ** pull bus low for Tlow0 (60us) then release
            */
            switch(port)
            {
#ifdef PORTA
                case DS_PORT_A:
                    DDRA = DDRA | phigh;
                    PORTA = PORTA & plow;
                    _delay_us(Tlow0);
                    DDRA = DDRA & plow;
                    PORTA = PORTA | phigh;  // Enable internal pull-up
                    break;
#endif
#ifdef PORTB
                case DS_PORT_B:
                    DDRB = DDRB | phigh;
                    PORTB = PORTB & plow;
                    _delay_us(Tlow0);
                    DDRB = DDRB & plow;
                    PORTB = PORTB | phigh;  // Enable internal pull-up
                    break;
#endif
#ifdef PORTC
                case DS_PORT_C:
                    DDRC = DDRC | phigh;
                    PORTC = PORTC & plow;
                    _delay_us(Tlow0);
                    DDRC = DDRC & plow;
                    PORTC = PORTC | phigh;  // Enable internal pull-up
                    break;
#endif
#ifdef PORTD
                case DS_PORT_D:
                    DDRD = DDRD | phigh;
                    PORTD = PORTD & plow;
                    _delay_us(Tlow0);
                    DDRD = DDRD & plow;
                    PORTD = PORTD | phigh;  // Enable internal pull-up
                    break;
#endif
                default:
                    break;
            }
        }
        else
        {
            /* write 1 */
            /* pull bus low for Tlow1, then release and wait rest of slot */
            switch(port)
            {
#ifdef PORTA
                case DS_PORT_A:
                    DDRA = DDRA | phigh;
                    PORTA = PORTA & plow;
                    _delay_us(Tlow1);
                    DDRA = DDRA & plow;
                    PORTA = PORTA | phigh;  // Enable internal pull-up
                    _delay_us(Tslot - Tlow1);
                    break;
#endif
#ifdef PORTB
                case DS_PORT_B:
                    DDRB = DDRB | phigh;
                    PORTB = PORTB & plow;
                    _delay_us(Tlow1);
                    DDRB = DDRB & plow;
                    PORTB = PORTB | phigh;  // Enable internal pull-up
                    _delay_us(Tslot - Tlow1);
                    break;
#endif
#ifdef PORTC
                case DS_PORT_C:
                    DDRC = DDRC | phigh;
                    PORTC = PORTC & plow;
                    _delay_us(Tlow1);
                    DDRC = DDRC & plow;
                    PORTC = PORTC | phigh;  // Enable internal pull-up
                    _delay_us(Tslot - Tlow1);
                    break;
#endif
#ifdef PORTD
                case DS_PORT_D:
                    DDRD = DDRD | phigh;
                    PORTD = PORTD & plow;
                    _delay_us(Tlow1);
                    DDRD = DDRD & plow;
                    PORTD = PORTD | phigh;  // Enable internal pull-up
                    _delay_us(Tslot - Tlow1);
                    break;
#endif
                default:
                    break;
            }
        }

        /* must wait Trec time after each bit is transmitted */
        _delay_us(Trec);

        ch = ch >> 1;
    }

    return 0;
}


uint8_t _read_byte(ds18b20_t *p)
{
    ds_port_t port;
    uint8_t   pin;
    uint8_t   plow;
    uint8_t   phigh;

    uint8_t n;
    uint8_t data;
    uint8_t bit;

    port  = p->port;
    pin   = p->pin;
    plow = p->plow;
    phigh = p->phigh;

    data = 0;

    for (n = 0; n < 8; n++)
    {
        // Initiate read time slot: pull low briefly
        switch(port)
        {
#ifdef PORTA
            case DS_PORT_A:
                DDRA = DDRA | phigh;
                PORTA = PORTA & plow;
                _delay_us(Tlow1);
                DDRA = DDRA & plow;
                PORTA = PORTA | phigh;  // Enable internal pull-up
                _delay_us(Trdv - Tlow1);
                bit = (PINA & phigh) >> pin;
                break;
#endif

#ifdef PORTB
            case DS_PORT_B:
                DDRB = DDRB | phigh;
                PORTB = PORTB & plow;
                _delay_us(Tlow1);
                DDRB = DDRB & plow;
                PORTB = PORTB | phigh;  // Enable internal pull-up
                _delay_us(Trdv - Tlow1);
                bit = (PINB & phigh) >> pin;
                break;
#endif

#ifdef PORTC
            case DS_PORT_C:
                DDRC = DDRC | phigh;
                PORTC = PORTC & plow;
                _delay_us(Tlow1);
                DDRC = DDRC & plow;
                PORTC = PORTC | phigh;  // Enable internal pull-up
                _delay_us(Trdv - Tlow1);
                bit = (PINC & phigh) >> pin;
                break;
#endif

#ifdef PORTD
            case DS_PORT_D:
                DDRD = DDRD | phigh;
                PORTD = PORTD & plow;
                _delay_us(Tlow1);
                DDRD = DDRD & plow;
                PORTD = PORTD | phigh;  // Enable internal pull-up
                _delay_us(Trdv - Tlow1);
                bit = (PIND & phigh) >> pin;
                break;
#endif

            default:
                bit = 0;
                break;
        }

        data |= (bit << n);

        /* Wait the rest of the slot + recovery */
        _delay_us(Tslot - Trdv + 1 + Trec);
    }

    return data;
}



/*
** nominally public routines
*/

uint8_t ds18b20_init(ds18b20_t *p)
{
/* struct *p must have port and pin values set before this function is called */
    uint8_t max[4] = {7, 7, 6, 7};

    /*
    ** after we configure mask values phigh and plow, we can use them as follows:
    **   (DDR | phigh) to set pin direction as output
    **   (PORT | phigh) to set pin level high
    **   (DDR & plow) to set pin direction as input
    **   (PORT & plow) to set pin level low
    */
    p->phigh = (1 << p->pin);
    p->plow = ~p->phigh;

    p->configvalid = FALSE;
    p->present = FALSE;

    switch(p->port)
    {
#ifdef PORTA
        /* port A */
        case DS_PORT_A:
            if (p->pin > max[DS_PORT_A])
                return EBADR;

            break;
#endif /* PORTA */

#ifdef PORTB
        /* port B */
        case DS_PORT_B:
#ifdef _AVR_IOTN84_H_
            max[DS_PORT_B] = 3;
#endif /* _AVR_IOTN84_H_ */
#ifdef _AVR_IOTN85_H_
            max[DS_PORT_B] = 5;
#endif /* _AVR_IOTN85_H_ */

            if (p->pin > max[DS_PORT_B])
                return EBADR;

            break;
#endif /* PORTB */

#ifdef PORTC
        /* port C */
        case DS_PORT_C:
            if (p->pin > max[DS_PORT_C])
                return EBADR;

            break;
#endif

#ifdef PORTD
        /* port D */
        case DS_PORT_D:
            if (p->pin > max[DS_PORT_D])
                return EBADR;

            break;
#endif

        /* invalid port */
        default:
            return EBADR;
            break;
    }

    p->present = _reset(p);
    return p->present;
}

int16_t ds18b20_read_temperature(ds18b20_t *p)
{
    int n;
    uint8_t dq;

    /* 1. Reset + presence check */
    p->present = _reset(p);
    if (!p->present)
        return ENOTPRESENT;

    /* 2. Start conversion */
    _write_byte(p, SKIP_ROM);
    _write_byte(p, CONVERT_T);

    /* 3. Wait for conversion (750ms max for 12-bit) */
    dq = 0;
    for (n = 0; n < 8 && dq == 0; n++) {
        _delay_us(Tconv / 8);
        dq = _read_dq(p);
    }

    /* 4. Reset again */
    if (!_reset(p))
        return ENOTPRESENT;  // Device disappeared mid-read → critical error

    /* 5. Read scratchpad */
    _write_byte(p, SKIP_ROM);
    _write_byte(p, READ_SCRATCHPAD);

    for (n = 0; n < SCRATCHPAD_SIZE; n++)
        p->scratchpad[n] = _read_byte(p);

    /* 6. CRITICAL: Validate CRC!!! */
    if (ds18b20_crc8(p->scratchpad, 8) != p->scratchpad[8]) {
        // Corrupted data — DO NOT TRUST
        return DS18B20_ERROR_CRC;  // Define this! e.g. -1000
    }

    /* Now we can trust the data */

    uint8_t resolution = p->scratchpad[CONFIG];
    uint8_t fmask = 0x0F;
    switch(resolution) {
        case DS_RES_2: fmask = 0x08; break;
        case DS_RES_4: fmask = 0x0C; break;
        case DS_RES_8: fmask = 0x0E; break;
        // DS_RES_16: fmask = 0x0F (default)
    }

    uint8_t fraction = p->scratchpad[TEMPLSB] & fmask;
    int8_t tempint = (p->scratchpad[TEMPLSB] >> 4) | (p->scratchpad[TEMPMSB] << 4);

    /* Out-of-range detection */
    if (tempint < -880 || tempint > 2000 || 
        tempint == 0x0550 ||      // 85°C power-on default
        tempint == -2048)         // erased scratchpad / glitch
    {
        return DS18B20_ERROR_OOR;
    }

    p->tempint = tempint;
    p->tempfrac = fraction;
    p->temphigh = p->scratchpad[THIGH];
    p->templow = p->scratchpad[TLOW];
    p->resolution = resolution;
    p->configvalid = TRUE;

    int16_t temp16 = (int16_t)tempint * 16 + fraction;
    return temp16;
}