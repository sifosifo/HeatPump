// coding: utf-8

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
#include "main.h"
#include "errors.h"
#include "callbacks.h"

uint8_t POST_status = 0;
uint8_t ActiveErrors = 0;
uint8_t CurrentState = OFF_LOCKED;
static uint32_t StateEntryTime = 0;
volatile uint8_t Process_1s = 0;	// flag indicating when to process 1s tasks

// Callbacks

static void on_error_detected(uint8_t error_code, uint8_t type)
{
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

static void on_event(uint8_t event_type, uint8_t current_state)
{
	
}

// State machine bussiness

static inline const char *GetStateName(uint8_t s)
{
	if (s > FATAL_ERROR) return "UNKNOWN";
	return (const char *)pgm_read_word(&state_names[s]);
}

// Dedicated function for state changes with debug //megaprintf
void ChangeState(uint8_t newState)
{
	uint32_t now = timer_GetTimestamp_s();
	uint32_t spent    = now - StateEntryTime;

	//megaprintf("STATE CHANGE: %s -> %s | spent %lu s | uptime %lu s\n", GetStateName(CurrentState), GetStateName(newState), (unsigned long)spent, (unsigned long)now);

	CurrentState = newState;
	StateEntryTime = now;
}

void ProcessStateMachine_s(void)
{
	uint16_t EventTimer_s;
	uint8_t PrimaryFlow_dcl;
	uint8_t SecondaryFlow_dcl;

	EventTimer_s = GetEventTimer_s();
	wdt_reset();
	switch(CurrentState)
	{
		case OFF_COOLDOWN:	// Let circulating pumps run for some time after compresor was turned off
			if(EventTimer_s>COMPRESSOR_COOLDOWN_PERIOD)
			{
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 1);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 1);
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
			PrimaryFlow_dcl = flow_GetFlow_dclmin(PRIMARY_SIDE);
			SecondaryFlow_dcl = flow_GetFlow_dclmin(SECONDARY_SIDE);

			if(GetTankTemperatureState()==TEMPERATURE_BELOW_THRESHOLD)
			{				
				//megaprintf("Heatpump ON, was off for %d seconds\n", EventTimer_s);
				ChangeState(ON_FLOW_CHECKING);			
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 0);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 0);
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
			PrimaryFlow_dcl = flow_GetFlow_dclmin(PRIMARY_SIDE);
			SecondaryFlow_dcl = flow_GetFlow_dclmin(SECONDARY_SIDE);
			
			if(EventTimer_s<FLOW_CHECKING_TIMEOUT_PERIOD)			
			{
				//megaprintf("Current flow: Primary: %d dcl/min Secondary: %d dcl/min\n", PrimaryFlow_dcl, SecondaryFlow_dcl);
				if(flow_WaterFlowNominal())
				{
					//megaprintf("************Flow checking OK:*************\n");
					//megaprintf("Actual/Desired flow after %ds\n", EventTimer_s);
					//megaprintf("Primary:\t%d/%d l/min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
					//megaprintf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
					SetRelayState(COMPRESSOR, 0);
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
				SetRelayState(COMPRESSOR, 1);
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 1);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 1);
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
				SetRelayState(COMPRESSOR, 1);				
				ClearEventTimer_s();
			}
			if(flow_WaterFlowNominal()==0)	// Stop heatpump in case of insufficient flow
			{
				SetRelayState(COMPRESSOR, 1);
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 1);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 1);
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
				SetRelayState(COMPRESSOR, 1);
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 1);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 1);
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
	flow_StorePulses_s();	// Just store impulses and process in ProcessFlow_s	
	timer_Tick();		// Maintain uptime timestamp
	Process_1s = 1;		// Trigger 1s tasks
	PORTB ^= (1 << PB0);
}

void init(void)
{	
	wdt_disable();

	CAN_Init();	// Initialize CAN interface
	
	for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) Init_Temperature(i);
	temp_SetHysteresisTemperature(0);
	temp_SetTargetTemperature(0);		// Set 0, which effectively disables it, needs to be started over CAN by setting correct value
	MeasureTemperature();	// Do measurement to flush random values
	_delay_ms(1000);			// Wait for sensors to stabilize
	MeasureTemperature();	// Do initial measurement to avoid having random values after bootup	
	//megaprintf("Init_Temperature\n");

	flow_Init();
	//megaprintf("Init_WaterFlow\n");
	
	Init_Relays();
	//megaprintf("Init_Relays\n");
	
	sei();
	uart_init();
	
	//megaprintf("--------------Booting----------------\n");
	register_error_callback(on_error_detected);		// Register callbacks	
	
	timer_Init(&Task_1000ms);
	//megaprintf("Init_Timer\n");	

	wdt_enable(WDTO_8S);
}

int main(void)
{
	uint8_t sensor_id = 0;	

	init();
	
	while (1)	// Idle loop
	{	
		if(Process_1s)	// Process 1s tasks outside of interrupt
		{
			Process_1s = 0;	// Reset flag

			flow_Process();	// Calculate flow from pulses
			sensor_id = MeasureTemperature();	// Delays everything by 1s !
			if(sensor_id==TEMPERATURE_SENSOR_COUNT)	// TEMPERATURE_SENSOR_COUNT means no sensor had issue with reading
			{
				CheckTemperatureRanges();
				ProcessStateMachine_s();
			}else
			{
				//megaprintf("Init_Temperature\n");
				Init_Temperature(sensor_id);		// try to recover temperature sensor
			}			
		}
		can_process();	// Send messages from queue	
	}	
	return 0;
}