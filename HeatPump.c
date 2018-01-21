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
	printf("Going off\n");
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
	static uint8_t ThermostatState = OFF_;

	switch(ThermostatState)
	{
		//case OFF_LOCKED:
		//	break;
		case OFF_:
			if(GetTankTemperatureState()==TEMPERATURE_BELOW_THRESHOLD)
			{				
				printf("Heatpump ON\n");
				ThermostatState = ON_;
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 0);
			}
			break;
		//case ON_LOCKED:
		//	break;
		case ON_:
			if(GetTankTemperatureState()==TEMPERATURE_ABOVE_THRESHOLD)
			{
				printf("Heatpump OFF\n");
				ThermostatState = OFF_;
				SetRelayState(PRIMARY_CIRCULATION_PUMP, 1);
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
	printf("Timer");
//	ProcessStateMachine_s();
//	timestamp = GetTimestamp();	
//	if(timestamp/10==0)
//	{
//		SendDebugMessage(0x10, (uint8_t*)timestamp);
//	}
}

int main(void)
{
	uart_init();	
	printf("--------------Booting----------------\n");
	Init_Temperature();
	printf("Init_Temperature\n");
	Init_WaterFlow();
	printf("Init_WaterFlow\n");
	Init_Relays();
	printf("Init_Relays\n");
	Init_Timer(&Task_1000ms);	
	printf("Init_Timer\n");
	//err = Init_ComInterface();	
	sei();
	MeasureTemperature();	
	printf("MeasureTemperature\n");
	//RunPOST();	
	while (1)	// Idle loop
	{		
		//err = MeasureTemperature();
		MeasureTemperature();
		#ifdef DEBUG
		CheckIfCANIsActive();	
		#endif
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
