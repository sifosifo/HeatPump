#include "Temperature.h"
#include <stdint.h>
#include <stdio.h>	// For printf
#include <avr/io.h>
#include <stdlib.h>
#include <util/delay.h>
#include "ds1820/ds18b20.h"
#include "HeatPump.h"

struct Tsensor
{
	uint8_t* PORT;
	uint8_t* DDR;
	uint8_t* PIN;
	uint8_t pin;
	uint8_t state;
	uint8_t error_counter;
	int16_t temperature;
};

struct Tsensor Tsensors[TEMPERATURE_SENSOR_COUNT] = {
	(uint8_t*)&PORTC, (uint8_t*)&DDRC, (uint8_t*)&PINC, 0, TEMPERATURE_SENSOR_NOT_CONNECTED, 0, 0,
	(uint8_t*)&PORTC, (uint8_t*)&DDRC, (uint8_t*)&PINC, 1, TEMPERATURE_SENSOR_NOT_CONNECTED, 0, 0,
	(uint8_t*)&PORTC, (uint8_t*)&DDRC, (uint8_t*)&PINC, 2, TEMPERATURE_SENSOR_NOT_CONNECTED, 0, 0,
	(uint8_t*)&PORTC, (uint8_t*)&DDRC, (uint8_t*)&PINC, 3, TEMPERATURE_SENSOR_NOT_CONNECTED, 0, 0,
	(uint8_t*)&PORTC, (uint8_t*)&DDRC, (uint8_t*)&PINC, 4, TEMPERATURE_SENSOR_NOT_CONNECTED, 0, 0,
	(uint8_t*)&PORTC, (uint8_t*)&DDRC, (uint8_t*)&PINC, 5, TEMPERATURE_SENSOR_NOT_CONNECTED, 0, 0};

uint16_t TargetTankTemperature = 25*16;
uint16_t TargetTankTemperatureHysteresis = 5*16;

#define TARGET_TANK_TEMPERATURE_LOW		TargetTankTemperature - TargetTankTemperatureHysteresis/2
#define TARGET_TANK_TEMPERATURE_HIGH	TargetTankTemperature + TargetTankTemperatureHysteresis/2

void Init_Temperature(void)
{
	uint8_t i;

	for(i=0; i<TEMPERATURE_SENSOR_COUNT; i++)
	{
		//Temperatures[i] = 0;
	}
}

uint8_t MeasureTemperature(void)
{
	uint8_t i;
	uint8_t err = 0;
	uint8_t result = 0;	
	for(i=0; i<TEMPERATURE_SENSOR_COUNT; i++)	// Start conversions
	{	
		err = ds18b20convert(Tsensors[i].PORT, Tsensors[i].DDR, Tsensors[i].PIN, ( 1 << Tsensors[i].pin ), NULL );				
		result += err;
		Tsensors[i].state = (err == DS18B20_OK) ? TEMPERATURE_SENSOR_OK : TEMPERATURE_SENSOR_NOT_CONNECTED;		
	}	
	_delay_ms( 1000 );		//Delay (sensor needs time to perform conversion)
	for(i=0; i<TEMPERATURE_SENSOR_COUNT; i++)	// Get measured temperatures
	{		
		if(Tsensors[i].state == TEMPERATURE_SENSOR_OK)
		{	// No need to read value if sensor is not present
			err = ds18b20read(Tsensors[i].PORT, Tsensors[i].DDR, Tsensors[i].PIN, ( 1 << Tsensors[i].pin ), NULL, &Tsensors[i].temperature);
			result += err;
			Tsensors[i].state = (err == DS18B20_OK) ? TEMPERATURE_SENSOR_OK : TEMPERATURE_SENSOR_NOT_CONNECTED;		
		}else
		{	// Sensor is not connected
			printf("Sensor %d not connected...\n", i);	
			if(Tsensors[i].error_counter < 255)
			{
				Tsensors[i].error_counter++;
			}
		}		
	}	
	return(result);	// number of errors detected / more or less number of sensors not connected
}

int16_t GetTemperature(uint8_t index)
{
	return(Tsensors[index].temperature);
}

uint16_t GetDeltaTemperature(uint8_t sensor_index)
{
	if(sensor_index==PRIMARY_SIDE)
	{
		return(Tsensors[PRIMARY_SIDE_INLET].temperature-Tsensors[PRIMARY_SIDE_OUTLET].temperature);
	}else if(sensor_index==SECONDARY_SIDE)
	{
		return(Tsensors[Tsensors[SECONDARY_SIDE_OUTLET].temperature-SECONDARY_SIDE_INLET].temperature);
	}else
	{
		return(0xFFFF);
	}
}
	
uint8_t GetTankTemperatureState(void)
{
	printf("Temperature %d \n", Tsensors[TANK_TOP].temperature/16);
	if(Tsensors[TANK_TOP].temperature<TARGET_TANK_TEMPERATURE_LOW)
	{
		printf("Temperature below range\n");
		return(TEMPERATURE_BELOW_THRESHOLD);
	}else if(Tsensors[TANK_TOP].temperature>TARGET_TANK_TEMPERATURE_HIGH)
	{
		printf("Temperature above range\n");
		return(TEMPERATURE_ABOVE_THRESHOLD);
	}else
	{
		printf("Temperature in range\n");
		return(TEMPERATURE_IN_RANGE);
	}	
}
