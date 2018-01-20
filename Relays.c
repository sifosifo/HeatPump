#include "Relays.h"
#include "uart.h"


struct relay Relays[RELAY_COUNT] = {
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 5, OFF, 0,
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 6, OFF, 0,
	(uint8_t*)&PORTD, (uint8_t*)&DDRD, (uint8_t*)&PIND, 7, OFF, 0};

void DriveRelay(uint8_t relay_id)
{
	if(Relays[relay_id].state == ON)
	{		
		*Relays[relay_id].PORT &= ~(1 << Relays[relay_id].pin);
	}else
	{
		*Relays[relay_id].PORT |= (1 << Relays[relay_id].pin);
	}	
	printf("R%d%d\n", relay_id, (~Relays[relay_id].state)&0x01);
}

void SetRelayState(uint8_t relay_id, uint8_t new_state)
{
	Relays[relay_id].state = new_state;
	DriveRelay(relay_id);
}

uint8_t GetRelayState(uint8_t relay_id)
{
	return(Relays[relay_id].state);
}

void Init_Relays(void)
{
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
	uint8_t read_output = 0;
	uint8_t i;

	for(i=0; i<RELAY_COUNT; i++)
	{
		if(((mask>>i) & 0x01) == 1)	// Drive output
		{
			SetRelayState(i, (output>>i) & 0x01);
		}
		read_output += Relays[i].state<<i;
	}
}
