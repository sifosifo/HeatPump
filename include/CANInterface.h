#include <stdint.h>

#define BASE_CAN_ID	0x100
#define NO_CHANGE_REQUESTED   255U     // writing 255 to parameter will result to no change
#define CAN_TX_QUEUE_SIZE  16

#define MCP2515_CS_DDR   DDRB
#define MCP2515_CS_PORT  PORTB
#define MCP2515_CS_PIN   PB2

uint8_t CAN_Init(void);
void SendBootupMessage(uint8_t debug_value);
void SendBootupMessage2(uint8_t debug_value);
void SendDebugMessage(uint8_t id, uint8_t debug_value[8]);
void CheckIfCANIsActive(void);

void can_SendErrorMsg(const uint8_t *data);  /* non-blocking */
void can_process(void);
