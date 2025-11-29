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

extern uint8_t POST_status;
extern uint8_t CurrentState;
extern uint8_t ActiveErrors;

#endif
