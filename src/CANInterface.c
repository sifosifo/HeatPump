#include "CANInterface.h"
#include <stdint.h>
#include <avr/io.h>
#include <string.h>		// memcpy
#include <avr/interrupt.h>
#include <avr/wdt.h>	// for WTD reset
#include <util/delay.h>
#include "can.h"
#include "Temperature.h"
#include "WaterFlow.h"
#include "Relays.h"
#include "main.h"
#include "uart.h"
#include "errors.h"
#include "Timer.h"

typedef enum {RESERVED, DRIVE_OUTPUT, READ_PRIMARY, READ_SECONDARY, 
	READ_TANK, READ_ENERGY, READ_COP, GET_STATUS, 
	READ_ACTIVE_ERRORS, READ_ERROR_HISTORY, PARAMETERS, TIME} actions_index;

typedef enum {TARGET_TEMP, HYSTERESIS_TEMP, RESET} parameter_index;

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

static volatile can_custom_t tx_queue[CAN_TX_QUEUE_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;
static volatile uint8_t tx_count = 0;

static volatile can_custom_t rx_queue[CAN_RX_QUEUE_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_count = 0;

volatile uint8_t dropped_tx_msg_count = 0;
volatile uint8_t dropped_rx_msg_count = 0;
volatile uint8_t tx_msg_count = 0;
volatile uint8_t rx_msg_count = 0;
volatile uint8_t interrupt_storm_count = 0;

can_custom_t msg_isr;	// Global instead of local in ISR to save stack space in case of rapid back to back ISR firing and eating through stack

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

void mcp2515_force_reset(void)
{
	crash_info.last_function = 5;
	SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR1) | (1<<SPR0);  // fosc/128 = very safe
    SPSR = 0; 

    // 1. Make sure CS pin is output and idle high
    MCP2515_CS_DDR  |= (1 << MCP2515_CS_PIN);   // change if you use different pin
    MCP2515_CS_PORT |= (1 << MCP2515_CS_PIN);

    // 2. Send the RESET instruction (0xC0)
    MCP2515_CS_PORT &= ~(1 << MCP2515_CS_PIN);   // CS low
    SPDR = 0xC0;
    while (!(SPSR & (1 << SPIF))) ;              // wait
    MCP2515_CS_PORT |= (1 << MCP2515_CS_PIN);    // CS high

    // 3. Wait — this delay is NON-NEGOTIABLE
    _delay_ms(20);   // 10 ms is enough, 20 ms gives extra safety
}

uint8_t can_Init(void)
{
	crash_info.last_function = 6;
	uint8_t result = 0;

	mcp2515_force_reset();

	result = can_init(BITRATE_250_KBPS);	// Initialize MCP2515
	
	if(result == 0)
	{	// Error - not possible to initialise		
//		//megaprintf("FAIL\n");
	}else
	{
//		//megaprintf("OK\n");	
		// Enable interrupt on pin change on PORTB
		PCICR |= (1<<PCIE0);
		//= PCIFR & (1<<PCIF1);	

		//PCMSK1 |= (1<<PCINT8);
		PCMSK0 |= (1<<1);
	}
	return(result);
}

void force_reset()
{
	//megaprintf("Reset requested");
    cli();              // Disable interrupts (important so nothing delays the WDT)
    wdt_enable(WDTO_15MS);          // Re-enable with shortest timeout
    while (1);  
}

void set_metadata(uint16_t id, uint8_t length, can_custom_t *msg)
{
	msg->id = id;
	msg->length = length;
	msg->flags.rtr = 0;
	msg->flags.extended = 0;
}

void tx_enqueue(const can_custom_t *msg)
{
	cli();
	if (tx_count < CAN_TX_QUEUE_SIZE)	// if queue not full
	{
		tx_queue[tx_head] = *msg; // struct copy
		tx_head = (tx_head + 1) % CAN_TX_QUEUE_SIZE;
		tx_count++;
	}else
	{
		dropped_tx_msg_count++;
	}
	sei();   // re-enable interrupts
}

static uint8_t tx_dequeue(can_custom_t *out)
{	
	cli(); // protect indices/counter during modification
	if (tx_count > 0)
	{	
	*out = tx_queue[tx_tail]; // struct copy
	tx_tail = (tx_tail + 1) % CAN_TX_QUEUE_SIZE;
	tx_count--;
	}else
	{
		sei();
		return 0;
	}
	sei();

	return 1;
}

static void rx_enqueue(const can_custom_t *msg)
{	
	cli();
	if (rx_count < CAN_RX_QUEUE_SIZE)
	{
		rx_queue[rx_head] = *msg; // struct copy is OK and fast here
		rx_head = (rx_head + 1) % CAN_RX_QUEUE_SIZE;
		rx_count++;
	}else
	{
		dropped_rx_msg_count++;
	}
	sei();
}

static uint8_t rx_dequeue(can_custom_t *out)
{
	cli();

	if (rx_count > 0)
	{
		*out = rx_queue[rx_tail]; // struct copy
		rx_tail = (rx_tail + 1) % CAN_RX_QUEUE_SIZE;
		rx_count--;
	}else
	{
		sei();
		return 0;
	}
	sei();

	return 1;
}

ISR(PCINT0_vect)
{	
	if (can_get_message((can_t*)(&msg_isr))) rx_enqueue(&msg_isr);
	interrupt_storm_count++;
}

void can_save_crash(const crash_info_t *crash_info)
{
	can_custom_t msg;

	set_metadata(BASE_CAN_ID + READ_ERROR_HISTORY*2 + 1, 8, &msg);
	msg.data.word[0] = crash_info->t1;
	msg.data.word[1] = crash_info->t2;
	msg.data.word[2] = crash_info->t3;
	msg.data.byte[6] = crash_info->last_state;
	tx_enqueue(&msg);
}

void can_SendErrorMsg(const uint8_t *data)
{
	can_custom_t msg;

	set_metadata(BASE_CAN_ID + READ_ACTIVE_ERRORS*2 + 1, 8, &msg);		
	memcpy(msg.data.byte, data, 8);
	tx_enqueue(&msg);
}

void can_SaveRAM(const uint8_t *data)
{
	can_custom_t msg;

	set_metadata(BASE_CAN_ID - 1, 5, &msg);
	memcpy(msg.data.byte, data, 2);
	msg.data.byte[2] = tx_msg_count;
	msg.data.byte[3] = rx_msg_count;
	msg.data.byte[4] = interrupt_storm_count;
	tx_msg_count = 0;
	rx_msg_count = 0;
	interrupt_storm_count = 0;
	tx_enqueue(&msg);
}

void can_SendTimeMsg(void)
{
	can_custom_t msg;

	set_metadata(BASE_CAN_ID + TIME*2 + 1, 8, &msg);
	for(uint8_t i=0; i<3; i++)
	{
		msg.data.word[i] = timer_get_ms(i);
	}
	msg.data.byte[6] = dropped_tx_msg_count;
	msg.data.byte[7] = dropped_rx_msg_count;
	tx_enqueue(&msg);
}

/* Call this from main loop as fast as possible */
void can_process(void)
{
	can_custom_t msg;
	uint8_t tmp8;
	uint8_t i;

	/* Process rx messages */
	while (rx_dequeue(&msg))
	{
		rx_msg_count++;	
		// Even ID is request, Odd ID is reply
		// Request ID is BASE_CAN_ID + message_id * 2		
		switch(msg.id++)	// Switch uses id, incremented id is used for response
		{
		case BASE_CAN_ID+GET_STATUS*2:		
			msg.length = 3;
			msg.data.byte[0] = POST_status;
			msg.data.byte[1] = CurrentState;
			msg.data.byte[2] = ActiveErrors;
			tx_enqueue(&msg);
			break;
		case BASE_CAN_ID+DRIVE_OUTPUT*2:
			DriveOutputsByCAN(msg.data.byte[0], msg.data.byte[1]);
			msg.length = 1;
			msg.data.byte[0] = 0;	// clear response
			for(i = 0; i < RELAY_COUNT; i++)	// cycle through all relays and report their state
			{
				msg.data.byte[0] |= GetRelayState(i)<<i;
			}
			tx_enqueue(&msg);
			break;	
		case BASE_CAN_ID+READ_PRIMARY*2:
			msg.length = 8;		
			msg.data.word[0] = GetTemperature(PRIMARY_SIDE_INLET);		
			msg.data.word[1] = GetTemperature(PRIMARY_SIDE_OUTLET);
			msg.data.byte[4] = flow_GetFlow_dclmin(PRIMARY_SIDE);		
			msg.data.word[3] = 0;
			printf("e");
			tx_enqueue(&msg);			
			printf("f");
			break;
		case BASE_CAN_ID+READ_SECONDARY*2:
			msg.length = 8;
			msg.data.word[0] = GetTemperature(SECONDARY_SIDE_INLET);
			msg.data.word[1] = GetTemperature(SECONDARY_SIDE_OUTLET);
			msg.data.byte[4] = flow_GetFlow_dclmin(SECONDARY_SIDE);		
			msg.data.word[3] = 0;
			printf("g");
			tx_enqueue(&msg);			
			printf("h");
			break;
		case BASE_CAN_ID+READ_TANK*2:
			msg.length = 4;
			msg.data.word[0] = GetTemperature(TANK_TOP);
			msg.data.word[1] = GetTemperature(TANK_BOTTOM);
			printf("i");		
			tx_enqueue(&msg);
			printf("j");
			break;
		case BASE_CAN_ID+PARAMETERS*2:
			tmp8 = msg.data.byte[TARGET_TEMP];
			if(tmp8!=NO_CHANGE_REQUESTED)
			{
				printf("Set TT: %d\n", tmp8);
				temp_SetTargetTemperature(tmp8);
			}
			tmp8 = msg.data.byte[HYSTERESIS_TEMP];
			if(tmp8!=NO_CHANGE_REQUESTED)
			{
				printf("Set TH: %d\n", tmp8);
				temp_SetHysteresisTemperature(tmp8);
			}
			if(msg.data.byte[RESET]==0xA5)
			{
				printf("Requesting reset\n");
				error_Halt();
				force_reset();
				while(1) {}
			}
			msg.length = 2;
			msg.data.byte[0] = temp_GetTargetTemperature();
			msg.data.byte[1] = temp_GetHysteresisTemperature();		
			tx_enqueue(&msg);
			break;
		default:
			break;
		}
		printf("k");
	}

	/* Process tx messages */
	while ((tx_count > 0) && can_check_free_buffer())
	{
		tx_msg_count++;
		tx_dequeue(&msg);  // safe because we checked tx_count > 0
		can_send_message((can_t*)(&msg));
	}
}