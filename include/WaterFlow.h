
#define	PRIMARY_MIN_FLOW	14*10
#define	SECONDARY_MIN_FLOW	5*10

typedef enum {PRIMARY_SIDE, SECONDARY_SIDE, SENSOR_COUNT} sensor_index;

#define FLOW_SENSOR_COUNT SENSOR_COUNT

// Initialise all variables
void flow_Init(void);

// Get current flow
uint8_t flow_GetFlow_dclmin(uint8_t SensorIndex);

// Calculate flow every second
void flow_Process(void);

// Called at exact time intervals
void flow_StorePulses_s(void);

// Is flow on both sides nominal (above minimal)
uint8_t flow_WaterFlowNominal(void);
