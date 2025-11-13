
typedef enum {
	CAN,
	TEMPERATURE,
	GENERAL,
	RELAY,
	TIMER,
	MODULE_COUNT
} module;

#define MODULE_ID	0
#define ERROR_ID	1

void error_SetError(uint8_t ModuleID, uint8_t error_code);
void error_Halt(void);
