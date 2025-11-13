#include <stdint.h>

typedef enum {	PRIMARY_SIDE_INLET,		PRIMARY_SIDE_OUTLET,
				SECONDARY_SIDE_INLET,	SECONDARY_SIDE_OUTLET,
				TANK_TOP,				TANK_BOTTOM,
				TEMPERATURE_SENSOR_COUNT} temperature_sensor_index;

typedef enum {	TEMPERATURE_SENSOR_OK,	TEMPERATURE_SENSOR_NOT_CONNECTED,				
				TEMPERATURE_SENSOR_STATE_COUNT} state;

typedef enum {	TEMPERATURE_BELOW_THRESHOLD, TEMPERATURE_IN_RANGE, TEMPERATURE_ABOVE_THRESHOLD,
				TEMPERATURE_STATE_COUNT} temp_state;


void Init_Temperature(void);
uint8_t MeasureTemperature(void);
void CheckTemperatureRanges(void);
int16_t GetTemperature(uint8_t index);
int16_t GetDeltaTemperature(uint8_t sensor_index);
uint8_t GetTankTemperatureState(void);

