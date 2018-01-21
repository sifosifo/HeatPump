
#define	PRIMARY_MIN_FLOW	14*10
#define	SECONDARY_MIN_FLOW	5*10

// Initialise all variables
void Init_WaterFlow(void);

// Get current flow
uint8_t GetFlow_dclmin(uint8_t SensorIndex);

uint16_t GetPower_W(uint8_t SensorIndex);

// Calculate flow every second
void ProcessFlow_s(void);
