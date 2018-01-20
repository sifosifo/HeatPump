#include <stdint.h>

uint8_t Init_ComInterface(void);
void SendBootupMessage(uint8_t debug_value);
void SendBootupMessage2(uint8_t debug_value);
void SendDebugMessage(uint8_t id, uint8_t debug_value[8]);
