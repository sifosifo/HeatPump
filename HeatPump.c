// coding: utf-8

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <util/delay.h>

#include "Temperature.h"
#include "WaterFlow.h"
#include "Relays.h"
#include "ComInterface.h"
#include "Timer.h"
#include "uart.h"
#include "HeatPump.h"

#define BOOT_DELAY 100

uint8_t POST_status = 0;
uint8_t ActiveErrors = 0;

void Thermostat(void);

void Halt(void)
{	// Something must went wrong
	printf("Going off\n");
	while(1)
	{
		cli();
		DDRB = 0;
		DDRC = 0;
		DDRD = 0;
	}
}

ProcessStateMachine_s(void)
{
	static uint8_t CurrentState = MACHINE_OK;

	switch(CurrentState)
	{
		case MACHINE_OK:
			Thermostat();
			break;
		case OFF_:
			break;
		case ON_LOCKED:
			break;
		case ERROR:
			break;
		default:
			break;
	}
}

void Thermostat(void)
{
	static uint8_t ThermostatState = OFF_LOCKED;
	uint16_t EventTimer_s;
	uint8_t PrimaryFlow_dcl;
	uint8_t SecondaryFlow_dcl;

	EventTimer_s = GetEventTimer_s();
	switch(ThermostatState)
	{
		case OFF_COOLDOWN:	// Let circulating pumps run for some time after compresor was turned off
			if(EventTimer_s>COMPRESSOR_COOLDOWN_PERIOD)
			{
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 1);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 1);
				ThermostatState = OFF_LOCKED;
			}
			break;
		case OFF_LOCKED:	// No checking for temperature, needs to stay OFF for defined period of time
			if(EventTimer_s>CYCLING_PROTECTION_PERIOD_OFF)
			{
				ThermostatState = OFF_;
			}
			break;
		case OFF_:			// Check temperature and change state if needed
			if(GetTankTemperatureState()==TEMPERATURE_BELOW_THRESHOLD)
			{				
				printf("Heatpump ON, was off for %d seconds\n", EventTimer_s);
				ThermostatState = ON_FLOW_CHECKING;				
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 0);
				SetRelayState(SECONDARY_CIRCULATION_PUMP, 0);
				ClearEventTimer_s();
			}
			if(WaterFlowNominal())	// Block heatpump in case of nominal flow detected
			{	// Circulating pump relay is stuck or error while reading flow sensor
				// Might cause reading nominal flow when no flow is present - critical error
				printf("************Flow checking error:*************\n");
				printf("Actual/Desired flow \n");
				printf("Primary:\t%d/0l /min\n", PrimaryFlow_dcl/10);
				printf("Secondary:\t%d/0 l/min\n", SecondaryFlow_dcl/10);
				Halt();
			} 	
			break;
		case ON_FLOW_CHECKING:			
			PrimaryFlow_dcl = GetFlow_dclmin(PRIMARY_SIDE);
			SecondaryFlow_dcl = GetFlow_dclmin(SECONDARY_SIDE);
			if(EventTimer_s<FLOW_CHECKING_TIMEOUT_PERIOD)			
			{
				printf("Current flow: Primary: %d dcl/min Secondary: %d dcl/min\n", PrimaryFlow_dcl, SecondaryFlow_dcl);
				if(WaterFlowNominal())
				{
					printf("************Flow checking OK:*************\n");
					printf("Actual/Desired flow after %ds\n", EventTimer_s);
					printf("Primary:\t%d/%d l/min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
					printf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
					SetRelayState(COMPRESSOR, 0);
					ThermostatState = ON_LOCKED;
				}
			}else
			{
				printf("************Flow checking error:*************\n");
				printf("Actual/Desired flow after %ds timeout\n", FLOW_CHECKING_TIMEOUT_PERIOD);
				printf("Primary:\t%d/%dl /min\n", PrimaryFlow_dcl/10, PRIMARY_MIN_FLOW/10);
				printf("Secondary:\t%d/%d l/min\n", SecondaryFlow_dcl/10, SECONDARY_MIN_FLOW/10);
				Halt();
			}
			break;
		case ON_LOCKED:		// No checking for temperature, needs to stay ON for defined period of time
			if(EventTimer_s>CYCLING_PROTECTION_PERIOD_ON)
			{
				ThermostatState = ON_;
			}
			if(WaterFlowNominal()==0) ThermostatState = OFF_LOCKED;	// Stop heatpump in case of insufficient flow
			break;
		case ON_:			// Check temperature and change state if needed
			if(GetTankTemperatureState()==TEMPERATURE_ABOVE_THRESHOLD)
			{
				printf("Heatpump OFF, was on for %d seconds\n", EventTimer_s);
				ThermostatState = OFF_COOLDOWN;
				printf("Waiting for compressor cooldown for %ds\n", COMPRESSOR_COOLDOWN_PERIOD);
				SetRelayState(COMPRESSOR, 1);				
				ClearEventTimer_s();
			}
			if(WaterFlowNominal()==0) ThermostatState = OFF_LOCKED;	// Stop heatpump in case of insufficient flow
			break;
		default:
			printf("Error Thermostat.\n");
			Halt();
			break;
	}
}

void Task_1000ms(void)
{
	uint32_t timestamp;

	ProcessFlow_s();	
//	ProcessStateMachine_s();
//	timestamp = GetTimestamp();	
//	if(timestamp/10==0)
//	{
//		SendDebugMessage(0x10, (uint8_t*)timestamp);
//	}
}

int main(void)
{	
	Init_Temperature();
	printf("Init_Temperature\n");
	Init_WaterFlow();
	printf("Init_WaterFlow\n");
	Init_Relays();
	printf("Init_Relays\n");
	Init_Timer(&Task_1000ms);	
	printf("Init_Timer\n");		
	sei();
	uart_init();	
//	UDR0 = 'V';
	printf("--------------Booting----------------\n");	
	//RunPOST();	
	while (1)	// Idle loop
	{		
		
		#ifndef DEBUG
		CheckIfCANIsActive();	
		#endif
		//err = MeasureTemperature();
		MeasureTemperature();
		CheckTemperatureRanges();
	/*
		- Check if temperature sensors present -> TempSensPresent
		- If TempSensPresent, check if temperature in range -> TempOK		
		- CAN initialization is not correct, after power reset (also CAN board has power reset), all ok
		After button/SW reset, CAN initialization fails
		- reset is caused by connecting? dissconecting? absent? serial connection
		- secondary power value is not correct - too high
		- values at web page are not updated all the time
		*/
		ProcessStateMachine_s();				
	}	
	return 0;
}
 
