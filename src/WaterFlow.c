#include <avr/interrupt.h>
#include <stdio.h>
#include "WaterFlow.h"

uint8_t Pulses[FLOW_SENSOR_COUNT];		// Runtime pulses  - ongoing counting
uint8_t Pulses_[FLOW_SENSOR_COUNT];		// Captured pulses - input for processing
uint8_t Flows_dclmin[FLOW_SENSOR_COUNT];

void flow_Init(void)
{
	uint8_t i;

	// Reset variables
	for(i = 0; i++; i < FLOW_SENSOR_COUNT)
	{
		Pulses[i] = 0;
		Pulses_[i] = 0;
		Flows_dclmin[i] = 0;
	}

	// Set trigger to rising edge of INT0 and INT1
	EICRA |=
		((1<<ISC00) | (1<<ISC01)) |		// Set INT0
		((1<<ISC10) | (1<<ISC11));		// Set INT1

	// Enable interrupts
	EIMSK |= (1<<INT0) | (1<<INT1);
}

uint8_t flow_GetFlow_dclmin(uint8_t SensorIndex)
{
	if(SensorIndex<FLOW_SENSOR_COUNT)
	{
		return(Flows_dclmin[SensorIndex]);
	}else
	{
		return(0xFF);
	}
}

void flow_StorePulses_s(void)		// Do minimum in interrupt - just store
{
	for(uint8_t i = 0; i < FLOW_SENSOR_COUNT; i++)
	{
		Pulses_[i] = Pulses[i];
		Pulses[i] = 0;	// reset counter
	}
}

//	Sensor range:	0-25	l/min
//					0-165	Hz
//	F[Hz] = 6,6 * Q[l/min]
//	pulses / 1s = 6,6 * Q[l/min]
//	Q[l/min]	= pulses / 6,6s
//	Q[dcl/min]	= pulses * 10 / 6,6s
//	Q[dcl/min]	= pulses * 100 / 66s
void flow_Process(void)
{
	for(uint8_t i = 0; i < FLOW_SENSOR_COUNT; i++)
	{
		Flows_dclmin[i] = (uint8_t)(((uint16_t)Pulses_[i] * (uint16_t)50) / (uint16_t)33);
	}
	////megaprintf("PFR:\t%d, SFR:\t%d\n", Pulses_[0], Pulses_[1]);
}

uint8_t flow_WaterFlowNominal(void)
{
	uint8_t nominal = 0;
	uint8_t PrimaryFlow_dcl;
	uint8_t SecondaryFlow_dcl;

	PrimaryFlow_dcl = flow_GetFlow_dclmin(PRIMARY_SIDE);
	SecondaryFlow_dcl = flow_GetFlow_dclmin(SECONDARY_SIDE);
	if((PrimaryFlow_dcl>PRIMARY_MIN_FLOW)&&(SecondaryFlow_dcl>SECONDARY_MIN_FLOW))
	{
		nominal = 1;
	}
	
	return(nominal);
}

ISR(INT0_vect)
{
	Pulses[PRIMARY_SIDE]++;
}

ISR(INT1_vect)
{
	Pulses[SECONDARY_SIDE]++;
}
