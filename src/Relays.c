#include "Relays.h"
#include "uart.h"
#include "main.h"
#include "callbacks.h"

struct relay_t Relays[RELAY_COUNT] = {
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 5, OFF, 0,
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 6, OFF, 0,
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 7, OFF, 0,
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 4, OFF, 0,};

void DriveRelay(uint8_t relay_id)
{
	crash_info.last_function = 17;
	if(Relays[relay_id].state == ON)
	{		
		*Relays[relay_id].PORT &= ~(1 << Relays[relay_id].pin);
	}else
	{
		*Relays[relay_id].PORT |= (1 << Relays[relay_id].pin);
	}	
	//megaprintf("R%d%d\n", relay_id, (~Relays[relay_id].state)&0x01);
}

void SetRelayState(uint8_t relay_id, uint8_t new_state)
{
	crash_info.last_function = 18;
	Relays[relay_id].state = new_state;
	DriveRelay(relay_id);
}


uint8_t GetRelayState(uint8_t relay_id)
{
	crash_info.last_function = 19;
	return(Relays[relay_id].state);
}

void Init_Relays(void)
{
	crash_info.last_function = 20;
	uint8_t i;

	for(i=0; i<RELAY_COUNT; i++)
	{
		SetRelayState(i, OFF);				// All relays off	
		*Relays[i].DDR |= (1 << Relays[i].pin);	// Set as output
	}
}

// output = desired output
// mask 0 means ignore, 1 means drive output
uint8_t DriveOutputsByCAN(uint8_t output, uint8_t mask)
{
	crash_info.last_function = 21;
	uint8_t i;

	for(i=0; i<RELAY_COUNT; i++)
	{
		if(((mask>>i) & 0x01) == 1)	// Drive output
		{
			if(i == AUX)
			{
				//megaprintf("AUX:%d/n", output>>i);
				SetRelayState(i, (output>>i) & 0x01);
			}else
			{
				//megaprintf("OUT%s:%d", i, output>>i);
				//
			}
		}
	}
}
