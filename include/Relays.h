#ifndef RELAYS_H_
#define RELAYS_H_

#include <stdint.h>
#include <avr/io.h>



typedef enum {PRIMARY_CIRCULATION_PUMP, SECONDARY_CIRCULATION_PUMP, COMPRESSOR, RELAY_COUNT} relay_index;

#define ON	(uint8_t)0
#define OFF	(uint8_t)1

struct relay
{
	uint8_t* PORT;
	uint8_t* DDR;
	uint8_t* PIN;
	uint8_t pin;
	uint8_t state;
	uint8_t error_counter;	
};

void Init_Relays(void);
void SetRelayState(uint8_t relay_id, uint8_t new_state);
uint8_t GetRelayState(uint8_t relay_id);
uint8_t DriveOutputsByCAN(uint8_t output, uint8_t mask);

#endif
