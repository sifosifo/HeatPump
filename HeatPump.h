#ifndef HEATPUMP_H_
#define HEATPUMP_H_

typedef enum {POST, BOOT, BOOT_ERROR, OFF_LOCKED, OFF_, ON_LOCKED, ON_, ERROR} hp_state;

typedef enum {PRIMARY_SIDE, SECONDARY_SIDE, SENSOR_COUNT} sensor_index;

#define FLOW_SENSOR_COUNT SENSOR_COUNT

extern uint8_t POST_status;
extern uint8_t CurrentState;
extern uint8_t ActiveErrors;

#endif
