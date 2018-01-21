
#define CYCLING_PROTECTION_PERIOD_OFF	1*60
#define CYCLING_PROTECTION_PERIOD_ON	2*60
#define COMPRESSOR_COOLDOWN_PERIOD		5
#define FLOW_CHECKING_TIMEOUT_PERIOD	15

void Init_Timer(void *Task_1000ms);
uint32_t GetTimestamp(void);
uint16_t GetEventTimer_s(void);
void ClearEventTimer_s(void);
