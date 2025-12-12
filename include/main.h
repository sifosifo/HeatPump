#ifndef HEATPUMP_H_
#define HEATPUMP_H_

#include <avr/pgmspace.h>

typedef enum {	POST, 				BOOT, 		BOOT_ERROR, OFF_COOLDOWN, 	OFF_LOCKED, 		OFF_,
				ON_FLOW_CHECKING, 	ON_LOCKED, 	ON_,		MACHINE_OK, 	RECOVERABLE_ERROR, 	FATAL_ERROR} hp_state;

static const char * const state_names[] PROGMEM =
{
	"POST",
	"BOOT",
	"BOOT_ERROR",
	"OFF_COOLDOWN",
	"OFF_LOCKED",
	"OFF",
	"ON_FLOW_CHECKING",
	"ON_LOCKED",
	"ON",
	"MACHINE_OK",
	"RECOVERABLE_ERROR",
	"FATAL_ERROR"
};

typedef struct
{
    uint16_t t1;
	uint16_t t2;
	uint16_t t3;
	uint8_t last_state;
	uint8_t last_function;
	uint8_t mcusr;
	uint8_t sent;
} crash_info_t;

extern crash_info_t crash_info __attribute__((section(".noinit")));

extern uint8_t POST_status;
extern uint8_t CurrentState;
extern uint8_t ActiveErrors;

#endif
