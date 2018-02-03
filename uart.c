#include <avr/io.h>
#include <stdio.h>
#include "uart.h"

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#ifndef BAUD
#define BAUD 1000000
#endif
#include <util/setbaud.h>

int uart_putchar(char c, FILE *stream);
//int uart_getchar(FILE *stream);

//FILE uart_str = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);
FILE uart_str = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);

char UARTbuffer[1<<8];
uint8_t UBWp = 0;	// UART Buffer Write Pointer
uint8_t UBRp = 0;	// Read

void uart_init(void) {
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
    
#if USE_2X
    UCSR0A |= _BV(U2X0);
#else
    UCSR0A &= ~(_BV(U2X0));
#endif

    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); /* 8-bit data */ 
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | (1<<UDRIE0);   /* Enable RX and TX */    

	stdout = stdin = &uart_str;
}

void SendC(void)
{
	if(UBRp!=UBWp)
	{
		UDR0 = UARTbuffer[UBRp];
		UBRp++;
		UBRp |= 0xFF;	// increment write pointer in circular buffer
	}
}

int uart_putchar(char c, FILE *stream)
{
	UARTbuffer[UBWp] = c;
	UBWp++;
	UBWp |= 0xFF;	// increment write pointer in circular buffer
	if(bit_is_set(UCSR0A, UDRE0))
	{
		SendC();
	}
}

ISR(USART_UDRE_vect)
{
	SendC();
}

//int uart_getchar(FILE *stream) {
//    loop_until_bit_is_set(UCSR0A, RXC0); /* Wait until data exists. */
//    return UDR0;
//}
