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
uint8_t CurrentState = BOOT;
uint8_t ActiveErrors = 0;

void Halt(void)
{	// Something must went wrong
	while(1)
	{
		cli();
		DDRB = 0;
		DDRC = 0;
		DDRD = 0;
	}
}

uint8_t RunPOST()	// Power On Self Test
{
	CurrentState = POST;
	_delay_ms(1000);
	if(GetFlow_dclmin(PRIMARY_SIDE)!=0) POST_status = (1<<0);
	if(GetFlow_dclmin(SECONDARY_SIDE)!=0) POST_status += (1<<1);
	SetRelayState(PRIMARY_CIRCULATION_PUMP, ON);
	SetRelayState(SECONDARY_CIRCULATION_PUMP, ON);
	_delay_ms(5000);
	if(GetFlow_dclmin(PRIMARY_SIDE)<10) POST_status = (1<<2);
	if(GetFlow_dclmin(SECONDARY_SIDE)<10) POST_status += (1<<3);
	SetRelayState(PRIMARY_CIRCULATION_PUMP, OFF);
	SetRelayState(SECONDARY_CIRCULATION_PUMP, OFF);
	if(POST_status==0)
	{
		CurrentState = OFF_LOCKED;
	}else
	{
		CurrentState = BOOT_ERROR;
	}
}
/*
StartCirculatingPumps(void)
{
	uint16_t EventTimer_s;


	if(flow>min)
	{	
		store EventTimer_s 
	}
	EventTimer_s = GetEventTimer_s();
}*/

ProcessStateMachine_s(void)
{
	switch(CurrentState)
	{
		case OFF_LOCKED:
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
	int16_t TankTemperature;
	static uint8_t ThermostatState = 0;

	TankTemperature = GetTemperature(TANK_TOP);
	
	switch(ThermostatState)
	{
		//case OFF_LOCKED:
		//	break;
		case OFF_:
			if(GetTankTemperatureState()==TEMPERATURE_BELOW_THRESHOLD)
			{				
				printf("Heatpump ON\n");
				ThermostatState = ON_;
			}
			break;
		//case ON_LOCKED:
		//	break;
		case ON_:
			if(GetTankTemperatureState()==TEMPERATURE_ABOVE_THRESHOLD)
			{
				printf("Heatpump OFF\n");
				ThermostatState = OFF_;
			}
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
	ProcessStateMachine_s();
	timestamp = GetTimestamp();	
	if(timestamp/10==0)
	{
		SendDebugMessage(0x10, (uint8_t*)timestamp);
	}
}

int main(void)
{
	uint8_t err = 0;
	uint8_t wow[8];

	uart_init();	
	
	printf("B");
	Init_Temperature();
	printf("1");
	Init_WaterFlow();
	printf("2");
	Init_Relays();
	printf("3");
	Init_Timer(&Task_1000ms);
	printf("4");
	err = Init_ComInterface();
	wow[0] = 0;
	wow[1] = 0;
	if(err==0)SendDebugMessage(0xFF, wow);
	printf("5\n");	
	sei();
	err = MeasureTemperature();
	wow[0] = 1;
	wow[1] = err;
	if(err!=0)SendDebugMessage(0xFF, wow);
	RunPOST();	
	while (1)	// Idle loop
	{		
		err = MeasureTemperature();
		//UDR0='T';
		
		//SendBootupMessage(err);
		CheckIfCANIsActive();	
	/*
		- Check if temperature sensors present -> TempSensPresent
		- If TempSensPresent, check if temperature in range -> TempOK
		- Check if flow > min in case pump is running -> FlowOK
		- Want to start a pump? If FlowOK, store EventTimer_s -> PumpOk
		- Want to start compressor? If PumpOk and TempOk and ShortCycleOK -> Start Compressor			*/
		ProcessStateMachine_s();
		Thermostat();		
	}	
	return 0;
}

/*
POST	-> BOOT			-> OFF	-> ON_LOCKED	-> ON		-> OFF_LOCKED -|
							^----------------------------------------------|
								-> ERROR
		-> BOOT_ERROR


*/
