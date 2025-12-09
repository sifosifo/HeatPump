#ifndef COMINTERFACE_H_
#define COMINTERFACE_H_

#include <stdint.h>

#define BASE_CAN_ID	0x100
#define NO_CHANGE_REQUESTED   255U     // writing 255 to parameter will result to no change
#define CAN_TX_QUEUE_SIZE  16
#define CAN_RX_QUEUE_SIZE  8

#define MCP2515_CS_DDR   DDRB
#define MCP2515_CS_PORT  PORTB
#define MCP2515_CS_PIN   PB2

uint8_t CAN_Init(void);

void can_SendErrorMsg(const uint8_t *data);  /* non-blocking */
void can_process(void);
void can_rx_process(void);

#endif /* COMINTERFACE_H_ */