#include <stdint.h>

#define BASE_CAN_ID	0x100
#define NO_CHANGE   255     // writing 255 to parameter will result to no change

uint8_t Init_ComInterface(void);
void SendBootupMessage(uint8_t debug_value);
void SendBootupMessage2(uint8_t debug_value);
void SendDebugMessage(uint8_t id, uint8_t debug_value[8]);
void CheckIfCANIsActive(void);
