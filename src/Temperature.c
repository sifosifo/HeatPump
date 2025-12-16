#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "Temperature.h"
#include "callbacks.h"

/* ------------------------------------------------------------------
   Sensor table
   ------------------------------------------------------------------ */
struct Tsensor {
	uint8_t   pin;
	uint8_t   state;
	uint8_t   error_counter;
	int16_t   temperature;
	ds18b20_t ds;
};

static struct Tsensor Tsensors[TEMPERATURE_SENSOR_COUNT] = {
	{.pin = 0}, {.pin = 1}, {.pin = 2},
	{.pin = 3}, {.pin = 4}, {.pin = 5}
};

/* ------------------------------------------------------------------
   Target & ranges
   ------------------------------------------------------------------ */
uint16_t TargetTankTemperature          = 10 * 16;
uint16_t TargetTankTemperatureHysteresis = 2 * 16;
uint16_t TargetTankTemperatureHigh      = 0;
uint16_t TargetTankTemperatureLow       = 0;

#define MIN 0
#define MAX 1

//  Healthy temperature ranges, if sensor reads out of its range, something is wrong
static int8_t TemperatureRanges[TEMPERATURE_SENSOR_COUNT][2] =
{
	{-10, 30}, {-10, 30},
	{  5, 60}, {  5, 60},
	{  5, 60}, {  5, 60}
};

void recalculateThresholds(void)
{
	TargetTankTemperatureHigh = TargetTankTemperature + TargetTankTemperatureHysteresis/2;
	TargetTankTemperatureLow = TargetTankTemperature - TargetTankTemperatureHysteresis/2;
	//megaprintf("TT TL TH: %d, %d, %d\n", TargetTankTemperature/16, TargetTankTemperatureLow/16, TargetTankTemperatureHigh/16);
}

void temp_SetTargetTemperature(uint8_t value)
{
	TargetTankTemperature = value * 4;
	recalculateThresholds();
}

uint8_t temp_GetTargetTemperature(void)
{
	return(TargetTankTemperature/4);
}

void temp_SetHysteresisTemperature(uint8_t value)
{
	TargetTankTemperatureHysteresis = value * 4;
	recalculateThresholds();
}

uint8_t temp_GetHysteresisTemperature(void)
{
	return(TargetTankTemperatureHysteresis/4);
}

/* ------------------------------------------------------------------
   Init
   ------------------------------------------------------------------ */
void Init_Temperature(uint8_t error_sensor_id)
{
	printf("Init_Temp\n");
	if(error_sensor_id >= TEMPERATURE_SENSOR_COUNT)
	{
		notify_error(TEMPERATURE_SENSOR_COUNT, OTHER);
	}else
	{    
		Tsensors[error_sensor_id].ds.port = DS_PORT_C;
		Tsensors[error_sensor_id].ds.pin  = Tsensors[error_sensor_id].pin;

		uint8_t rc = ds18b20_init(&Tsensors[error_sensor_id].ds);
		Tsensors[error_sensor_id].state = (rc == 1) ? TEMPERATURE_SENSOR_OK : TEMPERATURE_SENSOR_NOT_CONNECTED;
		Tsensors[error_sensor_id].error_counter = (rc == 1) ? 0 : 1;
		//Tsensors[i].temperature = 0;

		if(Tsensors[error_sensor_id].state != TEMPERATURE_SENSOR_OK) notify_error(error_sensor_id, INIT_ERROR);
	}
}

/* ------------------------------------------------------------------
   MeasureTemperature – called **once per second**
   ------------------------------------------------------------------ */
uint8_t MeasureTemperature(void)
{
	uint8_t error_sensor_id = TEMPERATURE_SENSOR_COUNT; // means no sensor has error reading

//    //megaprintf("T:");

	for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) {
		if (Tsensors[i].state != TEMPERATURE_SENSOR_OK) {
//            //megaprintf(" NC");
			notify_error(i, NOT_CONNECTED);
			error_sensor_id = i;
			continue;
		}

		int16_t temp = ds18b20_read_temperature(&Tsensors[i].ds);

		// Filter out jumps to 0. It is likely bad reading - ignore it and report
/*        if((temp == 0) && (((Tsensors[i].temperature - temp) > 8) || ((Tsensors[i].temperature - temp) < 8)))
		{
			notify_error(i, ZERO);
			error_sensor_id++;
			continue;
		}
*/
		if (temp == ENOTPRESENT || temp == DS18B20_ERROR_CRC || temp == DS18B20_ERROR_OOR)
		{
			Tsensors[i].state = TEMPERATURE_SENSOR_NOT_CONNECTED;   // TODO: Other states not considered
			if (Tsensors[i].error_counter < 255) ++Tsensors[i].error_counter;

			if(temp == ENOTPRESENT) notify_error(i, NOT_CONNECTED);
			if(temp == DS18B20_ERROR_OOR) notify_error(i, OUT_OF_RANGE);
			if(temp == DS18B20_ERROR_CRC) notify_error(i, WRONG_CRC);
			error_sensor_id = i;
		}else
		{
			Tsensors[i].temperature = temp;
			Tsensors[i].error_counter = 0;
		}
	}
//    //megaprintf("\n");
	return error_sensor_id;
}

void CheckTemperatureRanges(void)
{
	for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i)
	{
		if (Tsensors[i].state != TEMPERATURE_SENSOR_OK) continue;

		int8_t t = Tsensors[i].temperature / 16;
		if (t < TemperatureRanges[i][MIN])
		{
			notify_error(i, TOO_LOW);
			//megaprintf("T%d LOW: %d < %d\n", i, t, TemperatureRanges[i][MIN]);
		}else if (t > TemperatureRanges[i][MAX])
		{
			notify_error(i, TOO_HIGH);
			//megaprintf("T%d HIGH: %d > %d\n", i, t, TemperatureRanges[i][MAX]);
		}
	}
}

int16_t GetTemperature(uint8_t index)
{
	return Tsensors[index].temperature;
}

uint8_t GetTankTemperatureState(void)
{
	_Bool start  = GetTemperature(TANK_BOTTOM) <= TargetTankTemperatureLow;

//	float dT_water = GetTemperature(SECONDARY_SIDE_OUTLET) - GetTemperature(SECONDARY_SIDE_INLET);   // secondary side ΔT
//    float power_now = flow_tank_side * dT_water * 1.163;   // approx. kW

	// Remember maximum power that occurred in the first 30–60 min of this run
//    if (heat_pump_running && power_now > peak_power_this_run) {
//        peak_power_this_run = power_now;
//    }

	// Stop when current power has dropped to ≤ 30–35 % of the peak power this run
 //   bool stop = (power_now <= peak_power_this_run * 0.33) ||
 //               (T_bottom_buffer >= TargetTankTemperature + 2.0);   // safety ceiling
 
	_Bool stop = (GetTemperature(TANK_BOTTOM) >= TargetTankTemperatureHigh);   // safety ceiling

	if(start) return TEMPERATURE_BELOW_THRESHOLD;
	if(stop) return TEMPERATURE_ABOVE_THRESHOLD;
	return TEMPERATURE_IN_RANGE;
}