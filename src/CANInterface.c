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

static can_custom_t tx_queue[CAN_TX_QUEUE_SIZE];
static volatile uint8_t tx_head = 0;
static volatile uint8_t tx_tail = 0;
static volatile uint8_t tx_count = 0;

/* ---------------- RX queue ---------------- */
static volatile can_custom_t rx_queue[CAN_RX_QUEUE_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_count = 0;

void can_SendMsgRaw(const can_custom_t *msg);

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

/* Called from ISR context: push received message to rx_queue.
   If queue is full, newest message is dropped. */
static void rx_enqueue_from_isr(const can_custom_t *m)
{
	crash_info.last_function = 8;
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
	crash_info.last_function = 9;
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
	crash_info.last_function = 10;
	can_custom_t msg;
	
	if (can_get_message((can_t*)(&msg))) {
        rx_enqueue_from_isr(&msg);
    }
	//printf("CAN isr\n");
}

void can_rx_process(void)
{
	crash_info.last_function = 11;
	can_custom_t msg;
	uint8_t tmp8;

	while (rx_dequeue(&msg))
	{
		printf("d");
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
			printf("e");
			can_SendMsgRaw(&msg);
			//can_send_message((can_t*)(&msg));
			printf("f");
			break;
		case BASE_CAN_ID+READ_SECONDARY*2:
			msg.length = 8;
			msg.data.word[0] = GetTemperature(SECONDARY_SIDE_INLET);
			msg.data.word[1] = GetTemperature(SECONDARY_SIDE_OUTLET);
			msg.data.byte[4] = flow_GetFlow_dclmin(SECONDARY_SIDE);		
			msg.data.word[3] = 0;
			printf("g");
			can_SendMsgRaw(&msg);
			//can_send_message((can_t*)(&msg));
			printf("h");
			break;
		case BASE_CAN_ID+READ_TANK*2:
			msg.length = 4;
			msg.data.word[0] = GetTemperature(TANK_TOP);
			msg.data.word[1] = GetTemperature(TANK_BOTTOM);
			printf("i");		
			can_SendMsgRaw(&msg);
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
			can_send_message((can_t*)(&msg));
			break;
		default:
			break;
		}
		printf("k");
	}
}

void can_save_crash(crash_info_t *crash_info)
{
	can_SendMsg(BASE_CAN_ID + READ_ERROR_HISTORY*2 + 1, (uint8_t*)crash_info, 8);
}

void can_SendErrorMsg(const uint8_t *data)
{
	crash_info.last_function = 13;
	can_SendMsg(BASE_CAN_ID + READ_ERROR_HISTORY*2 + 1, (uint8_t*)data, 8);
}

void can_SendTimeMsg(void)
{
	crash_info.last_function = 38;
	uint8_t data[6];
	uint16_t time_ms;

	time_ms = timer_get_ms(0);
	data[0] = (uint8_t)(time_ms >> 8);
	data[1] = (uint8_t)(time_ms & 0xFF);
	time_ms = timer_get_ms(1);
	data[2] = (uint8_t)(time_ms >> 8);
	data[3] = (uint8_t)(time_ms & 0xFF);
	time_ms = timer_get_ms(2);
	data[4] = (uint8_t)(time_ms >> 8);
	data[5] = (uint8_t)(time_ms & 0xFF);
	can_SendMsg(BASE_CAN_ID + TIME*2 + 1, data, 6);
}

void can_SendMsgRaw(const can_custom_t *msg)
{
	uint8_t next;

	tx_queue[tx_head].id = msg->id;
	tx_queue[tx_head].length = msg->length;

	// if queue full -> drop
	if (tx_count >= CAN_TX_QUEUE_SIZE) return;

	// copy into current head slot (atomicize modifications)
	cli();   // disable interrupts while we modify shared indices/counter
	memcpy(tx_queue[tx_head].data.byte, msg->data.byte, msg->length);

	next = (tx_head + 1) % CAN_TX_QUEUE_SIZE;
	tx_head = next;
	tx_count++;
	sei();   // re-enable interrupts
}

void can_SendMsg(uint16_t can_id, const uint8_t *data, uint8_t length)
{
	crash_info.last_function = 37;
	uint8_t next;

	tx_queue[tx_head].id = can_id;
	tx_queue[tx_head].length = length;

	// if queue full -> drop
	if (tx_count >= CAN_TX_QUEUE_SIZE) return;

	// copy into current head slot (atomicize modifications)
	cli();   // disable interrupts while we modify shared indices/counter
	memcpy(tx_queue[tx_head].data.byte, data, length);

	next = (tx_head + 1) % CAN_TX_QUEUE_SIZE;
	tx_head = next;
	tx_count++;
	sei();   // re-enable interrupts
}

/* Call this from main loop as fast as possible */
void can_process(void)
{
	crash_info.last_function = 14;
	can_custom_t msg;

	msg.id 				= tx_queue[tx_tail].id;
	msg.length			= tx_queue[tx_tail].length;
	msg.flags.extended 	= 0;
	msg.flags.rtr 		= 0;

	if (tx_count == 0) return;

	if (can_check_free_buffer()) 
	{
		printf("b");
		cli(); // protect tx_tail / tx_count
		can_custom_t *f = &tx_queue[tx_tail];
		// copy the slot into message payload
		for (uint8_t i = 0; i < msg.length; i++) msg.data.byte[i] = f->data.byte[i];

		tx_tail = (tx_tail + 1) % CAN_TX_QUEUE_SIZE;
		tx_count--;
		sei();
		printf("x");
		can_send_message((can_t*)(&msg));
	}
}