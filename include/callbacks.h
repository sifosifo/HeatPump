// app_callbacks.h  (or put it in errors.h if you prefer)
#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <stdint.h>

typedef enum
{
	TEMP_SENSOR_PI,
	TEMP_SENSOR_PO,
	TEMP_SENSOR_SI,
	TEMP_SENSOR_SO,
	TEMP_SENSOR_TT,
	TEMP_SENSOR_TB, // Temperature sensors match temperature_sensor_index in temperature module
	FLOW_SENSOR_P,
	FLOW_SENSOR_S,
	GENERAL,
	ERR_CODE_COUNT
} err_code_t;

typedef enum
{
    INIT_ERROR,
	TOO_HIGH,
	TOO_LOW,
	NOT_CONNECTED,
	OTHER,
	ERR_TYPE_COUNT
} err_type_t;

// This is the only thing modules like temperature.c are allowed to call
typedef void (*error_detected_cb_t)(uint8_t error_code, uint8_t type);

void register_error_callback(error_detected_cb_t cb);

void notify_error(uint8_t error_code, uint8_t type);   

#endif