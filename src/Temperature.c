/* --------------------------------------------------------------
   Temperature.c – 1 Hz using public ds18b20_read_temperature()
   -------------------------------------------------------------- */
#include "Temperature.h"
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "HeatPump.h"
#include "errors.h"

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
static int8_t TemperatureRanges[TEMPERATURE_SENSOR_COUNT][2] = {
    {-10, 30}, {-10, 30},
    {  5, 60}, {  5, 60},
    {  5, 60}, {  5, 60}
};

void    temp_SetTargetTemperature(uint8_t value)
{
    TargetTankTemperature = value * 4;
    TargetTankTemperatureHigh = TargetTankTemperature + TargetTankTemperatureHysteresis/2;
    TargetTankTemperatureLow = TargetTankTemperature + TargetTankTemperatureHysteresis/2;
    printf("TT TL TH: %d, %d, %d\n", TargetTankTemperature, TargetTankTemperatureLow, TargetTankTemperatureHigh);
}

uint8_t temp_GetTargetTemperature(void)
{
    return(TargetTankTemperature/4);
}

/* ------------------------------------------------------------------
   Init
   ------------------------------------------------------------------ */
void Init_Temperature(void)
{
    for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) {
        Tsensors[i].ds.port = DS_PORT_C;
        Tsensors[i].ds.pin  = Tsensors[i].pin;

        uint8_t rc = ds18b20_init(&Tsensors[i].ds);
        Tsensors[i].state = (rc == 1) ? TEMPERATURE_SENSOR_OK : TEMPERATURE_SENSOR_NOT_CONNECTED;
        Tsensors[i].error_counter = (rc == 1) ? 0 : 1;
        Tsensors[i].temperature = 0x8000;
    }
}

/* ------------------------------------------------------------------
   MeasureTemperature – called **once per second**
   ------------------------------------------------------------------ */
uint8_t MeasureTemperature(void)
{
    uint8_t total_errors = 0;

//    printf("T:");

    for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) {
        if (Tsensors[i].state != TEMPERATURE_SENSOR_OK) {
//            printf(" NC");
            ++total_errors;
            continue;
        }

        int16_t temp = ds18b20_read_temperature(&Tsensors[i].ds);

        if (temp == ENOTPRESENT) {
            Tsensors[i].state = TEMPERATURE_SENSOR_NOT_CONNECTED;
            Tsensors[i].temperature = 0x8000;
//            printf(" ERR");
            ++total_errors;
            if (Tsensors[i].error_counter < 255) ++Tsensors[i].error_counter;
        } else {
            Tsensors[i].temperature = temp;
            Tsensors[i].error_counter = 0;
//            printf(" %dC", temp / 16);
        }
    }
//    printf("\n");
    return total_errors;
}

/* ------------------------------------------------------------------
   Rest of your functions (copy from before)
   ------------------------------------------------------------------ */
void CheckTemperatureRanges(void)
{
    for (uint8_t i = 0; i < TEMPERATURE_SENSOR_COUNT; ++i) {
        if (Tsensors[i].state != TEMPERATURE_SENSOR_OK || Tsensors[i].temperature == 0x8000) continue;
        int8_t t = Tsensors[i].temperature / 16;
        if (t < TemperatureRanges[i][MIN])
            printf("T%d LOW: %d < %d\n", i, t, TemperatureRanges[i][MIN]);
        else if (t > TemperatureRanges[i][MAX])
            printf("T%d HIGH: %d > %d\n", i, t, TemperatureRanges[i][MAX]);
    }
}

int16_t GetTemperature(uint8_t index)
{
    if (Tsensors[index].state != TEMPERATURE_SENSOR_OK) return 0x8000;
    return Tsensors[index].temperature;
}

int16_t GetDeltaTemperature(uint8_t sensor_index)
{
    if (sensor_index == PRIMARY_SIDE) {
        int16_t in  = GetTemperature(PRIMARY_SIDE_INLET);
        int16_t out = GetTemperature(PRIMARY_SIDE_OUTLET);
        if (in == 0x8000 || out == 0x8000) return 0xFFFF;
        return in - out;
    }
    if (sensor_index == SECONDARY_SIDE) {
        int16_t out = GetTemperature(SECONDARY_SIDE_OUTLET);
        int16_t in  = GetTemperature(SECONDARY_SIDE_INLET);
        if (out == 0x8000 || in == 0x8000) return 0xFFFF;
        return out - in;
    }
    return 0xFFFF;
}

uint8_t GetTankTemperatureState(void)
{
    int16_t t = GetTemperature(TANK_TOP);
    if (t == 0x8000) {
//        printf("Tank sensor error!\n");
        return TEMPERATURE_IN_RANGE;
    }
//    printf("Tank temperature %dC\n", t / 16);
    if (t < TargetTankTemperatureLow) {
//        printf("Temperature below range\n");
        return TEMPERATURE_BELOW_THRESHOLD;
    }
    if (t > TargetTankTemperatureHigh) {
//        printf("Temperature above range\n");
        return TEMPERATURE_ABOVE_THRESHOLD;
    }
//    printf("Temperature in range\n");
    return TEMPERATURE_IN_RANGE;
}