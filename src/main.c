// coding: utf-8
#include "main.h"
crash_info_t crash_info __attribute__((section(".noinit")));

#include <avr/io.h>		//led_on
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>	// for WTD reset
#include <stdlib.h>
#include <util/delay.h>
#include <string.h>		// memcpy

#include "Temperature.h"
#include "WaterFlow.h"
#include "Relays.h"
#include "CANInterface.h"
#include "Timer.h"
#include "uart.h"
#include "errors.h"
#include "callbacks.h"

uint8_t POST_status = 0;
uint8_t ActiveErrors = 0;
uint8_t CurrentState = OFF_LOCKED;
static uint32_t StateEntryTime = 0;
volatile uint8_t Process_1s = 0;	// flag indicating when to process 1s tasks

extern uint8_t __heap_start, __bss_start, __bss_end;
extern void *__brkval;

//-- CRASH info --//
/*
void __attribute__((naked, section(".init3"))) clear_bss_manually(void) {
    // Default init3 clears .bss
    // We skip clearing our .noinit section
    uint8_t* p = &__bss_start;
    while (p != &__bss_end)
        *p++ = 0;
    // .noinit section is NOT cleared → survives warm reset
}*/
/*
static inline void wdt_enable_interrupt_and_reset(void)
{
    cli();
	wdt_reset();
    WDTCSR = (1<<WDCE) | (1<<WDE);
    WDTCSR = (1<<WDIE) | (1<<WDE) | (1<<WDP2) | (1<<WDP1);  // ~500 ms
    sei();
}*/

// This runs when WDT times out (but before the actual reset)
ISR(WDT_vect) {
    // We have only a few microseconds! Keep it tiny.
    crash_info.t1			= timer_get_ms(0);
	crash_info.t2     = timer_get_ms(1);
	crash_info.t3     = timer_get_ms(2);
    crash_info.mcusr        = MCUSR;
    
    // Optional: force even faster reset so we don't loop forever in ISR
    wdt_enable(WDTO_15MS);
}
//-- CRASH info --//

// Callbacks

static void on_error_detected(uint8_t error_code, uint8_t type)
{
	crash_info.last_function = 0;
	uint32_t now = timer_GetTimestamp_s();
	uint8_t payload[8];

	payload[0] = ((error_code & 0x0F) << 4) | (type & 0x0F);
	payload[1] = CurrentState;									// Current state machine state
	payload[2] = ((now - StateEntryTime) >> 8) & 0xFF;
	payload[3] = (now - StateEntryTime) & 0xFF;
	payload[4] = (uint8_t)(now >> 24);
	payload[5] = (uint8_t)(now >> 16);
	payload[6] = (uint8_t)(now >> 8);
	payload[7] = (uint8_t)(now >> 0);

	can_SendErrorMsg(payload);
}

/*
static void on_event(uint8_t event_type, uint8_t current_state)
{
	
}

// State machine bussiness
*/
static inline const char *GetStateName(uint8_t s)
{
	if (s > FATAL_ERROR) return "UNKNOWN";
	return (const char *)pgm_read_word(&state_names[s]);
}

// Dedicated function for state changes with debug //megaprintf
void ChangeState(uint8_t newState)
{
	crash_info.last_function = 1;
	uint32_t now = timer_GetTimestamp_s();
	uint32_t spent    = now - StateEntryTime;

	printf("STATE CHANGE: %s -> %s | spent %lu s | uptime %lu s\n", GetStateName(CurrentState), GetStateName(newState), (unsigned long)spent, (unsigned long)now);
	crash_info.last_state = CurrentState;
	CurrentState = newState;
	StateEntryTime = now;
}

void ProcessStateMachine_s(void)
{
	crash_info.last_function = 2;
	uint16_t EventTimer_s;
//	uint8_t PrimaryFlow_dcl;
//	uint8_t SecondaryFlow_dcl;

	EventTimer_s = GetEventTimer_s();
	switch(CurrentState)
	{
		case POST:
			break;
		case OFF_COOLDOWN:	// Let circulating pumps run for some time after compresor was turned off
			if(EventTimer_s>COMPRESSOR_COOLDOWN_PERIOD)
			{
				SetRelayState(PRIMARY_CIRCULATION_PUMP, OFF);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, OFF);
				ChangeState(OFF_LOCKED);
				ClearEventTimer_s();
			}
			break;
		case OFF_LOCKED:	// No checking for temperature, needs to stay OFF for defined period of time
			if(EventTimer_s>CYCLING_PROTECTION_PERIOD_OFF)
			{
				ChangeState(OFF_);
				ClearEventTimer_s();
			}
			break;
		case OFF_:			// Check temperature and change state if needed
//			PrimaryFlow_dcl = flow_GetFlow_dclmin(PRIMARY_SIDE);
//			SecondaryFlow_dcl = flow_GetFlow_dclmin(SECONDARY_SIDE);

			if(GetTankTemperatureState()==TEMPERATURE_BELOW_THRESHOLD)
			{				
				//megaprintf("Heatpump ON, was off for %d seconds\n", EventTimer_s);
				ChangeState(ON_FLOW_CHECKING);			
				SetRelayState(PRIMARY_CIRCULATION_PUMP, ON);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, ON);
				ClearEventTimer_s();
			}
			if(flow_WaterFlowNominal())	// Block heatpump in case of nominal flow detected
			{	// Circulating pump relay is stuck or error while reading flow sensor
				// Might cause reading nominal flow when no flow is present - critical error
				//megaprintf("************Flow checking error:*************\n");
				//megaprintf("Actual/Desired flow \n");
				//megaprintf("Primary:\t%d/0l /min\n", PrimaryFlow_dcl/10);
				//megaprintf("Secondary:\t%d/0 l/min\n", SecondaryFlow_dcl/10);
				//error_Halt();
				ChangeState(RECOVERABLE_ERROR);
				ClearEventTimer_s();
			} 	
			break;
		case ON_FLOW_CHECKING:			
//			PrimaryFlow_dcl = flow_GetFlow_dclmin(PRIMARY_SIDE);
//			SecondaryFlow_dcl = flow_GetFlow_dclmin(SECONDARY_SIDE);
			
			if(EventTimer_s<FLOW_CHECKING_TIMEOUT_PERIOD)			
			{
				//megaprintf("Current flow: Primary: %d dcl/min Secondary: %d dcl/min\n", PrimaryFlow_dcl, SecondaryFlow_dcl);
				if(flow_WaterFlowNominal())
				{
					//megaprintf("************Flow checking OK:*************\n");
					//megaprintf("Actual/Desired flow after %ds\n", EventTimer_s);
					//megaprintf("Primary:\t%d/%d l/min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
					//megaprintf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
					SetRelayState(COMPRESSOR, ON);
					ChangeState(ON_LOCKED);
					ClearEventTimer_s();
				}
			}else
			{
				//megaprintf("************Flow checking error:*************\n");
				//megaprintf("Actual/Desired flow after %ds timeout\n", FLOW_CHECKING_TIMEOUT_PERIOD);
				//megaprintf("Primary:\t%d/%dl /min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
				//megaprintf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
				//error_Halt();
				ChangeState(RECOVERABLE_ERROR);
				ClearEventTimer_s();
			}
			break;
		case ON_LOCKED:		// No checking for temperature, needs to stay ON for defined period of time
			if(EventTimer_s>CYCLING_PROTECTION_PERIOD_ON)
			{
				ChangeState(ON_);
				ClearEventTimer_s();
			}
			if(flow_WaterFlowNominal()==0)	// Stop heatpump in case of insufficient flow
			{
				SetRelayState(COMPRESSOR, OFF);
				SetRelayState(PRIMARY_CIRCULATION_PUMP, OFF);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, OFF);
				ChangeState(OFF_LOCKED);
				ClearEventTimer_s();
			}
			break;
		case ON_:			// Check temperature and change state if needed
			if(GetTankTemperatureState()==TEMPERATURE_ABOVE_THRESHOLD)
			{
				//megaprintf("Heatpump OFF, was on for %d seconds\n", EventTimer_s);
				ChangeState(OFF_COOLDOWN);
				//megaprintf("Waiting for compressor cooldown for %ds\n", COMPRESSOR_COOLDOWN_PERIOD);
				SetRelayState(COMPRESSOR, OFF);				
				ClearEventTimer_s();
			}
			if(flow_WaterFlowNominal()==0)	// Stop heatpump in case of insufficient flow
			{
				SetRelayState(COMPRESSOR, OFF);
				SetRelayState(PRIMARY_CIRCULATION_PUMP, OFF);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, OFF);
				ChangeState(OFF_LOCKED);
				ClearEventTimer_s();
			}
			break;
		case RECOVERABLE_ERROR:			
			if(EventTimer_s > RECOVERABLE_ERROR_PERIOD_ON)
			{
				ChangeState(OFF_LOCKED);
				ClearEventTimer_s();
			}else
			{
				SetRelayState(COMPRESSOR, OFF);
				SetRelayState(PRIMARY_CIRCULATION_PUMP, OFF);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, OFF);
			}
			break;
		case FATAL_ERROR:
			//megaprintf("Fatal error Thermostat.\n");
			error_Halt();
			notify_error(7, 0);
			break;
		default:
			//megaprintf("Non-existent state %d.\n", CurrentState);
			error_Halt();
			notify_error(7, 1);
			break;
	}
}

void Task_1000ms(void)
{
	crash_info.last_function = 3;
	flow_StorePulses_s();	// Just store impulses and process in ProcessFlow_s	
	Process_1s = 1;		// Trigger 1s tasks
	PORTB ^= (1 << PB0);
}

void paint_stack(void)
{
    uint8_t *p;

    uint8_t *heap_end = (__brkval == 0 ? &__heap_start : (uint8_t *)__brkval);

    for (p = heap_end; p <= (uint8_t*)RAMEND - 16; p++) {
        *p = 0xAA;
    }
}

uint16_t get_min_free_ram(void)
{
    uint8_t *heap_end;
    uint8_t *p;

    // Determine heap end
    heap_end = (__brkval == 0 ? &__heap_start : (uint8_t *)__brkval);

    // Scan upward until we find the *first byte the stack has overwritten*
    for (p = heap_end; p <= (uint8_t*)RAMEND; p++)
    {
        if (*p != 0xAA)   // stack has reached here at some point
            break;
    }

    // p is now the first overwritten byte
    // free RAM = distance between heap and this point
    return (uint16_t)(p - heap_end);
}

void init(void)
{
	crash_info.mcusr = MCUSR;
	crash_info.sent = 0;
	MCUSR = 0;  // clear flags
	wdt_disable();

	can_save_crash(&crash_info);
	can_Init();	// Initialize CAN interface
	
	uart_init();
	for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) Init_Temperature(i);
	temp_SetHysteresisTemperature(0);
	temp_SetTargetTemperature(0);		// Set 0, which effectively disables it, needs to be started over CAN by setting correct value
	MeasureTemperature();	// Do measurement to flush random values
	_delay_ms(1000);			// Wait for sensors to stabilize
	MeasureTemperature();	// Do initial measurement to avoid having random values after bootup
	printf("Init_Temperature\n");

	flow_Init();
	printf("Init_WaterFlow\n");
	
	Init_Relays();
	printf("Init_Relays\n");
	paint_stack();
	sei();
	
	printf("--------------Booting----------------\n");
	register_error_callback(on_error_detected);		// Register callbacks	
	
	timer_Init(&Task_1000ms);
	printf("Init_Timer\n");	

	wdt_enable(WDTO_2S);
}

int main(void)
{
	uint8_t sensor_id;

	init();
	printf("B");
	while (1)	// Idle loop
	{	
		wdt_reset();
		
		if(Process_1s)	// Process 1s tasks outside of interrupt
		{
			Process_1s = 0;	// Reset flag
			printf("1");
			timer_start_ms(0);
			if(CurrentState != POST)
			{
				printf("2");
				flow_Process();	// Calculate flow from pulses
				timer_start_ms(2);
				sensor_id = MeasureTemperature();	// Delays everything by 1s !
				timer_stop_ms(2);
				printf("8");
				if(sensor_id==TEMPERATURE_SENSOR_COUNT)	// TEMPERATURE_SENSOR_COUNT means no sensor had issue with reading
				{
					printf("3");
					CheckTemperatureRanges();
					printf("9");
					ProcessStateMachine_s();
					printf("0");
				}else
				{
					printf("7");
					//megaprintf("Init_Temperature\n");
					Init_Temperature(sensor_id);		// try to recover temperature sensor
				}
			}
			printf("4");
			timer_stop_ms(0);
			printf("5");
			can_SendTimeMsg();
			printf("6");
			uint16_t stack_usage = get_min_free_ram();
			uint8_t stack_usage_bytes[2];
			stack_usage_bytes[0] = (uint8_t)(stack_usage >> 8);
			stack_usage_bytes[1] = (uint8_t)(stack_usage & 0xFF);
			can_SaveRAM(stack_usage_bytes);

			uint32_t timestamp = timer_GetTimestamp_s();
			if(timestamp % 3600 == 0)	// In future based on CAN silence timer
			{
				can_Init();	// Re-initialize CAN to recover from potential errors
			}
		}

		timer_start_ms(1);		
		can_process();	// Send error messages from queue		
		crash_info.sent = 1;
		timer_stop_ms(1);
	}	
	return 0;
}