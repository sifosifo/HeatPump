#include "ComInterface.h"
#include <stdint.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "can.h"
#include "Temperature.h"
#include "WaterFlow.h"
#include "Relays.h"
#include "HeatPump.h"
#include "uart.h"

typedef enum {RESERVED, DRIVE_OUTPUT, READ_PRIMARY, READ_SECONDARY, 
	READ_TANK, READ_ENERGY, READ_COP, GET_STATUS, 
	READ_ACTIVE_ERRORS, READ_ERROR_HISTORY} actions_index;

typedef struct
{
	uint32_t id;				//!< ID der Nachricht (11 oder 29 Bit)
	struct {
		int rtr : 1;			//!< Remote-Transmit-Request-Frame?
		int extended : 1;		//!< extended ID?
	} flags;
	
	uint8_t length;				//!< Anzahl der Datenbytes
	union
	{
		uint8_t byte[8];		// access data as array of 8 bit variables
		uint16_t word[4];		// access data as array of 16 bit variables
	} data;

} can_custom_t;

// -----------------------------------------------------------------------------
/** Set filters and masks.
 *
 * The filters are divided in two groups:
 *
 * Group 0: Filter 0 and 1 with corresponding mask 0.
 * Group 1: Filter 2, 3, 4 and 5 with corresponding mask 1.
 *
 * If a group mask is set to 0, the group will receive all messages.
 *
 * If you want to receive ONLY 11 bit identifiers, set your filters
 * and masks as follows:
 *
 *	uint8_t can_filter[] PROGMEM = {
 *		// Group 0
 *		MCP2515_FILTER(0),				// Filter 0
 *		MCP2515_FILTER(0),				// Filter 1
 *		
 *		// Group 1
 *		MCP2515_FILTER(0),				// Filter 2
 *		MCP2515_FILTER(0),				// Filter 3
 *		MCP2515_FILTER(0),				// Filter 4
 *		MCP2515_FILTER(0),				// Filter 5
 *		
 *		MCP2515_FILTER(0),				// Mask 0 (for group 0)
 *		MCP2515_FILTER(0),				// Mask 1 (for group 1)
 *	};
 *
 *
 * If you want to receive ONLY 29 bit identifiers, set your filters
 * and masks as follows:
 *
 * \code
 *	uint8_t can_filter[] PROGMEM = {
 *		// Group 0
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 0
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 1
 *		
 *		// Group 1
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 2
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 3
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 4
 *		MCP2515_FILTER_EXTENDED(0),		// Filter 5
 *		
 *		MCP2515_FILTER_EXTENDED(0),		// Mask 0 (for group 0)
 *		MCP2515_FILTER_EXTENDED(0),		// Mask 1 (for group 1)
 *	};
 * \endcode
 *
 * If you want to receive both 11 and 29 bit identifiers, set your filters
 * and masks as follows:
 */
const uint8_t can_filter[] PROGMEM = 
{
	// Group 0
	MCP2515_FILTER(0),				// Filter 0
	MCP2515_FILTER(0),				// Filter 1
	
	// Group 1
	MCP2515_FILTER_EXTENDED(0),		// Filter 2
	MCP2515_FILTER_EXTENDED(0),		// Filter 3
	MCP2515_FILTER_EXTENDED(0),		// Filter 4
	MCP2515_FILTER_EXTENDED(0),		// Filter 5
	
	MCP2515_FILTER(0),				// Mask 0 (for group 0)
	MCP2515_FILTER_EXTENDED(0),		// Mask 1 (for group 1)
};
// You can receive 11 bit identifiers with either group 0 or 1.

void SendBootupMessage(uint8_t debug_value)
{	
	can_t msg;
	
	msg.id = 0x123;
	msg.flags.rtr = 0;
	msg.flags.extended = 0;
	
	msg.length = 5;
	msg.data[0] = 0xde;
	msg.data[1] = 0xad;
	msg.data[2] = 0xbe;
	msg.data[3] = 0xef;
	msg.data[4] = debug_value;
	
	can_send_message(&msg);
}

void SendBootupMessage2(uint8_t debug_value)
{	
	can_t msg;
	
	msg.id = 0x130;
	msg.flags.rtr = 0;
	msg.flags.extended = 0;
	
	msg.length = 5;
	msg.data[0] = 0xde;
	msg.data[1] = 0xad;
	msg.data[2] = 0xbe;
	msg.data[3] = 0xef;
	msg.data[4] = debug_value;
	
	can_send_message(&msg);
}

void SendDebugMessage(uint8_t id, uint8_t debug_value[8])
{	
	can_t msg;
	
	msg.id = id;
	msg.flags.rtr = 0;
	msg.flags.extended = 0;
	
	msg.length = 8;
	msg.data[0] = debug_value[0];
	msg.data[1] = debug_value[1];
	msg.data[2] = debug_value[2];
	msg.data[3] = debug_value[3];
	msg.data[4] = debug_value[4];
	msg.data[5] = debug_value[5];
	msg.data[6] = debug_value[6];
	msg.data[7] = debug_value[7];
	
	can_send_message(&msg);
}

uint8_t Init_ComInterface(void)
{
	uint8_t result = 0;
	printf("CAN\n");
	// Initialize MCP2515
	result = can_init(BITRATE_250_KBPS);
	
	if(result == 0)
	{	// Error - not possible to initialise		
		printf("FAIL\n");
	}else
	{
		printf("OK\n");
	}
	// Baudrate constants are in library in file mcp2515.c
	
	// Load filters and masks
//	can_static_filter(can_filter);
	
	// Enable interrupt on pin change on PORTB
	PCICR |= (1<<PCIE0);
	//= PCIFR & (1<<PCIF1);	

	//PCMSK1 |= (1<<PCINT8);
	PCMSK0 |= (1<<1);

	return(result);
}

void CheckIfCANIsActive(void)
{
	static uint8_t CANactive = 0;	// Indicates if CAN was initialized correctly
	uint8_t err;

	if(CANactive==0)
	{
		err = Init_ComInterface();
		if(err==0)
		{	
			CANactive = 0;
		}else
		{
			CANactive = 1;
		}
	}
}

ISR(PCINT0_vect)
{
	can_custom_t msg;	
	int16_t tmp;

	can_get_message((can_t*)(&msg));
	//can_get_message(&msg);
	//can_send_message(&msg);

	// Even ID is request, Odd ID is reply
	// Request ID is BASE_CAN_ID + message_id * 2
	// Response ID is simply incremented
	switch(msg.id++)
	{
	case BASE_CAN_ID+GET_STATUS*2:		
		msg.length = 3;
		msg.data.byte[0] = POST_status;
		msg.data.byte[1] = CurrentState;
		msg.data.byte[2] = ActiveErrors;
		can_send_message((can_t*)(&msg));
		break;
	case BASE_CAN_ID+DRIVE_OUTPUT*2:
		msg.length = 1;
		msg.data.byte[0] = DriveOutputsByCAN(msg.data.byte[1], msg.data.byte[2]);
		can_send_message((can_t*)(&msg));
		break;	
	case BASE_CAN_ID+READ_PRIMARY*2:
		msg.length = 8;		
		msg.data.word[0] = GetTemperature(PRIMARY_SIDE_INLET);		
		msg.data.word[1] = GetTemperature(PRIMARY_SIDE_OUTLET);
		msg.data.byte[4] = GetFlow_dclmin(PRIMARY_SIDE);		
		msg.data.word[3] = GetPower_W(PRIMARY_SIDE);
		can_send_message((can_t*)(&msg));
		break;
	case BASE_CAN_ID+READ_SECONDARY*2:
		msg.length = 8;
		msg.data.word[0] = GetTemperature(SECONDARY_SIDE_INLET);
		msg.data.word[1] = GetTemperature(SECONDARY_SIDE_OUTLET);
		msg.data.byte[4] = GetFlow_dclmin(SECONDARY_SIDE);		
		msg.data.word[3] = GetPower_W(SECONDARY_SIDE);
		can_send_message((can_t*)(&msg));
		break;
	case BASE_CAN_ID+READ_TANK*2:
		msg.length = 4;
		msg.data.word[0] = GetTemperature(TANK_TOP);
		msg.data.word[1] = GetTemperature(TANK_BOTTOM);		
		can_send_message((can_t*)(&msg));
		break;
	default:
		break;
	}
}



