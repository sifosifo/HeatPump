//#include <stdint.h>
#include <stdio.h>	// For printf to UART
#include <avr/io.h>	// Access to ports for Halt function
#include <avr/interrupt.h>	// cli()
#include "errors.h"

uint8_t ErrorsHistory[100][2];
uint8_t ErrorsActive[10][2];
uint8_t ActiveErrorCount = 0;

void error_Halt(void);

void error_SetError(uint8_t ModuleID, uint8_t error_code)
{
	uint8_t i;
	uint8_t Found = 0;

	for(i = 0; (i<ActiveErrorCount) && (Found==0); i++)
	{
		if(ErrorsActive[i][MODULE_ID]==ModuleID)
		{
			if(ErrorsActive[i][ERROR_ID])
			{
				Found = 1;
			}
		}
	}

	if(Found)
	{
		
	}else
	{
		ActiveErrorCount++;

	}
}

void error_Halt(void)
{       // Something must went wrong
	printf("Going off\n");
	while(1)
	{
		cli();
		DDRB = 0;
		DDRC = 0;
		DDRD = 0;
	}
}

