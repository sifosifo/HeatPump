#include <avr/interrupt.h>
#include "WaterFlow.h"
#include "Constants.h"
#include "HeatPump.h"
#include "ComInterface.h"

uint8_t Pulses[FLOW_SENSOR_COUNT];
uint8_t Flows_dclmin[FLOW_SENSOR_COUNT];
uint16_t Power_W[FLOW_SENSOR_COUNT];

void Init_WaterFlow(void)
{
	uint8_t i;

	// Reset variables
	for(i = 0; i++; i < FLOW_SENSOR_COUNT)
	{
		Pulses[i] = 0;
		Flows_dclmin[i] = 0;
	}

	// Set trigger to rising edge of INT0 and INT1
	EICRA |=
		((1<<ISC00) | (1<<ISC01)) |		// Set INT0
		((1<<ISC10) | (1<<ISC11));		// Set INT1

	// Enable interrupts
	EIMSK |= (1<<INT0) | (1<<INT1);
}

uint8_t GetFlow_dclmin(uint8_t SensorIndex)
{
	if(SensorIndex<FLOW_SENSOR_COUNT)
	{
		return(Flows_dclmin[SensorIndex]);
	}else
	{
		return(0xFF);
	}
}

uint16_t GetPower_W(uint8_t SensorIndex)
{
	if(SensorIndex<FLOW_SENSOR_COUNT)
	{
		return(Power_W[SensorIndex]);
	}else
	{
		return(0xFF);
	}
}

//	Sensor range:	0-25	l/min
//					0-165	Hz
//	F[Hz] = 6,6 * Q[l/min]
//	pulses / 1s = 6,6 * Q[l/min]
//	Q[l/min]	= pulses / 6,6s
//	Q[dcl/min]	= pulses * 10 / 6,6s
//	Q[dcl/min]	= pulses * 100 / 66s
void ProcessFlow_s(void)
{
	uint8_t i;
	uint32_t tmp;
	int16_t deltaT;
	
	SendBootupMessage2(Pulses[0]);
	//UDR0 = 'F';

	for(i = 0; i < FLOW_SENSOR_COUNT; i++)
	{
		Flows_dclmin[i] = (uint8_t)(((uint16_t)Pulses[i] * (uint16_t)50) / (uint16_t)33);
		//Flows_dclmin[i] *= 10;SecondaryFlow_dcl
		//tmp = (uint32_t)Pulses * (uint32_t)[kg/m3] * (uint32_t)WaterSpecHeatCap[temperature] / (uint32_t)110;

		deltaT = GetDeltaTemperature(i);
		if(deltaT<0)deltaT=0;
		//deltaT = 4*16;
		
		tmp = (uint32_t)Pulses[i] * (uint32_t)980;
		tmp *= (uint32_t)4200;
		tmp /= (uint32_t)110;	
		Power_W[i]	=  (uint16_t)(tmp * (uint32_t)deltaT / (uint32_t)3600)/(uint16_t)(16);
		//Power_W[i]	=  (uint16_t)(tmp * (uint32_t)deltaT / (uint32_t)360)/(uint16_t)(16);
		Pulses[i] = 0;	// reset counter
	}
}

uint8_t WaterFlowNominal(void)
{
	uint8_t nominal = 0;
	uint8_t PrimaryFlow_dcl;
	uint8_t SecondaryFlow_dcl;

	PrimaryFlow_dcl = GetFlow_dclmin(PRIMARY_SIDE);
	SecondaryFlow_dcl = GetFlow_dclmin(SECONDARY_SIDE);
	if((PrimaryFlow_dcl>PRIMARY_MIN_FLOW)&&(SecondaryFlow_dcl>SECONDARY_MIN_FLOW))
	{
		nominal = 1;
	}

	return(nominal);
}

ISR(INT0_vect)
{
	//UDR0='W';
    Pulses[PRIMARY_SIDE]++;
}

ISR(INT1_vect)
{
	//UDR0='V';
    Pulses[SECONDARY_SIDE]++;
}
