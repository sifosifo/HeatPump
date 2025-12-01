#include "ComInterface.h"
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

typedef enum {RESERVED, DRIVE_OUTPUT, READ_PRIMARY, READ_SECONDARY, 
	READ_TANK, READ_ENERGY, READ_COP, GET_STATUS, 
	READ_ACTIVE_ERRORS, READ_ERROR_HISTORY, PARAMETERS} actions_index;

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

static uint8_t err_tx_queue[CAN_TX_QUEUE_SIZE][8];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;
static volatile uint8_t tx_count = 0;

/* ---------------- RX queue ---------------- */
static volatile can_custom_t rx_queue[CAN_RX_QUEUE_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_count = 0;

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

uint8_t can_Init(void)
{
	uint8_t result = 0;
//	printf("CAN\n");	
	result = can_init(BITRATE_250_KBPS);	// Initialize MCP2515
	
	if(result == 0)
	{	// Error - not possible to initialise		
//		printf("FAIL\n");
	}else
	{
//		printf("OK\n");	
		// Enable interrupt on pin change on PORTB
		PCICR |= (1<<PCIE0);
		//= PCIFR & (1<<PCIF1);	

		//PCMSK1 |= (1<<PCINT8);
		PCMSK0 |= (1<<1);
	}
	// Baudrate constants are in library in file mcp2515.c
	
	// Load filters and masks
//	can_static_filter(can_filter);

	return(result);
}

void can_CheckIfCANIsActive(void)
{
	static uint8_t CANactive = 0;	// Indicates if CAN was initialized correctly
	uint8_t err;
		
	if(CANactive==0)
	{
		printf("Init CAN\n");
		err = can_Init();
		printf("Init status: %d\n", err);
		if(err==0)
		{	
			CANactive = 0;
		}else
		{
			CANactive = 1;
		}
	}
}

void force_reset()
{
	printf("Reset requested");
    cli();              // Disable interrupts (important so nothing delays the WDT)
//    wdt_enable(WDTO_15MS);  // Enable watchdog with shortest timeout (15 ms)
//	while (1);
//asm volatile ("jmp 0");

    MCUSR = 0;                      // IMPORTANT: clear reset flags
    wdt_disable();                  // Disable WDT completely
    wdt_enable(WDTO_15MS);          // Re-enable with shortest timeout
    while (1);  
}

/* Called from ISR context: push received message to rx_queue.
   If queue is full, newest message is dropped. */
static void rx_enqueue_from_isr(const can_custom_t *m)
{
	uint8_t next = (rx_head + 1) % CAN_RX_QUEUE_SIZE;

	/* if queue full (next == tail and count==full) -> drop */
	if (rx_count >= CAN_RX_QUEUE_SIZE) {
		// drop newest message; you could instead overwrite oldest by advancing tail
		return;
	}

	/* copy into head */
	rx_queue[rx_head] = *m; // struct copy is OK and fast here
	rx_head = next;
	rx_count++;
}

/* Called from main loop: pop one message (returns 1 if popped) */
static uint8_t rx_dequeue(can_custom_t *out)
{
	if (rx_count == 0) return 0;

	cli(); // protect indices/counter during modification
	*out = rx_queue[rx_tail]; // struct copy
	rx_tail = (rx_tail + 1) % CAN_RX_QUEUE_SIZE;
	rx_count--;
	sei();

	return 1;
}

ISR(PCINT0_vect)
{
	can_custom_t msg;
	
	if (can_get_message((can_t*)(&msg))) {
        rx_enqueue_from_isr(&msg);
    }
	//printf("CAN isr\n");
}

void can_rx_process(void)
{
	can_custom_t msg;
	uint8_t tmp8;

	while (rx_dequeue(&msg))
	{

		// Even ID is request, Odd ID is reply
		// Request ID is BASE_CAN_ID + message_id * 2
		// Response ID is simply incremented
		switch(msg.id++)
		{
		case BASE_CAN_ID+GET_STATUS*2:		
			msg.length = 3;
			msg.data.byte[0] = POST_status;
			msg.data.byte[1] = 0;//TODO update frame CurrentState;
			msg.data.byte[2] = ActiveErrors;
			can_send_message((can_t*)(&msg));
			break;
		case BASE_CAN_ID+DRIVE_OUTPUT*2:
			msg.length = 1;
			msg.data.byte[0] = DriveOutputsByCAN(msg.data.byte[0], msg.data.byte[1]);
			can_send_message((can_t*)(&msg));
			break;	
		case BASE_CAN_ID+READ_PRIMARY*2:
			msg.length = 8;		
			msg.data.word[0] = GetTemperature(PRIMARY_SIDE_INLET);		
			msg.data.word[1] = GetTemperature(PRIMARY_SIDE_OUTLET);
			msg.data.byte[4] = flow_GetFlow_dclmin(PRIMARY_SIDE);		
			msg.data.word[3] = 0;
			can_send_message((can_t*)(&msg));
			break;
		case BASE_CAN_ID+READ_SECONDARY*2:
			msg.length = 8;
			msg.data.word[0] = GetTemperature(SECONDARY_SIDE_INLET);
			msg.data.word[1] = GetTemperature(SECONDARY_SIDE_OUTLET);
			msg.data.byte[4] = flow_GetFlow_dclmin(SECONDARY_SIDE);		
			msg.data.word[3] = 0;
			can_send_message((can_t*)(&msg));
			break;
		case BASE_CAN_ID+READ_TANK*2:
			msg.length = 4;
			msg.data.word[0] = GetTemperature(TANK_TOP);
			msg.data.word[1] = GetTemperature(TANK_BOTTOM);		
			can_send_message((can_t*)(&msg));
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
			can_send_message((can_t*)(&msg));
			break;
		default:
			break;
		}
	}
}

void can_SendErrorMsg(const uint8_t *data)
{
	uint8_t next;

	// if queue full -> drop
	if (tx_count >= CAN_TX_QUEUE_SIZE) return;

	// copy into current head slot (atomicize modifications)
	cli();   // disable interrupts while we modify shared indices/counter
	memcpy(err_tx_queue[tx_head], data, 8);

	next = (tx_head + 1) % CAN_TX_QUEUE_SIZE;
	tx_head = next;
	tx_count++;
	sei();   // re-enable interrupts
}

/* Call this from main loop as fast as possible */
void can_process(void)
{
	can_custom_t msg;

	msg.id 				= BASE_CAN_ID + READ_ACTIVE_ERRORS*2 + 1;
	msg.length			= 8;
	msg.flags.extended 	= 0;
	msg.flags.rtr 		= 0;

	if (tx_count == 0) return;

	if (can_check_free_buffer()) 
	{
		cli(); // protect tx_tail / tx_count
		uint8_t *f = err_tx_queue[tx_tail];
		// copy the slot into message payload
		for (uint8_t i = 0; i < 8; i++) msg.data.byte[i] = f[i];

		tx_tail = (tx_tail + 1) % CAN_TX_QUEUE_SIZE;
		tx_count--;
		sei();
		
		can_send_message((can_t*)(&msg));
	}
}