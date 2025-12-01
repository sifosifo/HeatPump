// coding: utf-8

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>	// for WTD reset
#include <stdlib.h>
#include <util/delay.h>
#include <string.h>		// memcpy

#include "Temperature.h"
#include "WaterFlow.h"
#include "Relays.h"
#include "ComInterface.h"
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

// Dedicated function for state changes with debug printf
void ChangeState(uint8_t newState)
{
	uint32_t now = timer_GetTimestamp_s();
	uint32_t spent    = now - StateEntryTime;

	printf("STATE CHANGE: %s -> %s | spent %lu s | uptime %lu s\n",
		GetStateName(CurrentState),
		GetStateName(newState),
		(unsigned long)spent,
		(unsigned long)now);

	CurrentState = newState;
	StateEntryTime = now;
}

void ProcessStateMachine_s(void)
{
	uint16_t EventTimer_s;
	uint8_t PrimaryFlow_dcl;
	uint8_t SecondaryFlow_dcl;

	EventTimer_s = GetEventTimer_s();
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
				printf("Heatpump ON, was off for %d seconds\n", EventTimer_s);
				ChangeState(ON_FLOW_CHECKING);			
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 0);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 0);
				ClearEventTimer_s();
			}
			if(flow_WaterFlowNominal())	// Block heatpump in case of nominal flow detected
			{	// Circulating pump relay is stuck or error while reading flow sensor
				// Might cause reading nominal flow when no flow is present - critical error
				printf("************Flow checking error:*************\n");
				printf("Actual/Desired flow \n");
				printf("Primary:\t%d/0l /min\n", PrimaryFlow_dcl/10);
				printf("Secondary:\t%d/0 l/min\n", SecondaryFlow_dcl/10);
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
				printf("Current flow: Primary: %d dcl/min Secondary: %d dcl/min\n", PrimaryFlow_dcl, SecondaryFlow_dcl);
				if(flow_WaterFlowNominal())
				{
					printf("************Flow checking OK:*************\n");
					printf("Actual/Desired flow after %ds\n", EventTimer_s);
					printf("Primary:\t%d/%d l/min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
					printf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
					SetRelayState(COMPRESSOR, 0);
					ChangeState(ON_LOCKED);
					ClearEventTimer_s();
				}
			}else
			{
				printf("************Flow checking error:*************\n");
				printf("Actual/Desired flow after %ds timeout\n", FLOW_CHECKING_TIMEOUT_PERIOD);
				printf("Primary:\t%d/%dl /min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
				printf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
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
				printf("Heatpump OFF, was on for %d seconds\n", EventTimer_s);
				ChangeState(OFF_COOLDOWN);
				printf("Waiting for compressor cooldown for %ds\n", COMPRESSOR_COOLDOWN_PERIOD);
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
			printf("Fatal error Thermostat.\n");
			error_Halt();
			notify_error(7, 0);
			break;
		default:
			printf("Non-existent state %d.\n", CurrentState);
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
}

int main(void)
{
	uint8_t sensor_id = 0;	

	wdt_disable();
	timer_Init(&Task_1000ms);
	printf("Init_Timer\n");
	
	for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) Init_Temperature(i);
	MeasureTemperature();	// Do initial measurement to avoid having random values after bootup
	printf("Init_Temperature\n");

	flow_Init();
	printf("Init_WaterFlow\n");
	
	Init_Relays();
	printf("Init_Relays\n");
	
	sei();
	uart_init();
	
	printf("--------------Booting----------------\n");
	register_error_callback(on_error_detected);		// Register callbacks
	//RunPOST();
	temp_SetHysteresisTemperature(0);
	temp_SetTargetTemperature(0);		// Set 0, which effectively disables it, needs to be started over CAN by setting correct value
	while (1)	// Idle loop
	{		
		#ifndef DEBUG
		CheckIfCANIsActive();	
		#endif
		if(Process_1s)
		{
			flow_Process();	// Calculate flow from pulses
			sensor_id = MeasureTemperature();	// Delays everything by 1s !
			if(sensor_id==TEMPERATURE_SENSOR_COUNT)	// TEMPERATURE_SENSOR_COUNT means no sensor had issue with reading
			{
				CheckTemperatureRanges();
				ProcessStateMachine_s();
			}else
			{
				printf("Init_Temperature\n");
				Init_Temperature(sensor_id);		// try to recover temperature sensor
			}
			Process_1s = 0;	// Reset flag
		}
		can_process();	// Send messages from queue	
	}	
	return 0;
}
 
