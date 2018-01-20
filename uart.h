#ifndef UART_H_
#define UART_H_
#include <stdio.h>

void uart_putchar(char c, FILE *stream);
char uart_getchar(FILE *stream);

void uart_init(void);


#endif
