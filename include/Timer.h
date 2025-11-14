
#define CYCLING_PROTECTION_PERIOD_OFF	6	*60
#define CYCLING_PROTECTION_PERIOD_ON	20	*60
#define COMPRESSOR_COOLDOWN_PERIOD		5
#define FLOW_CHECKING_TIMEOUT_PERIOD	10
#define	RECOVERABLE_ERROR_PERIOD_ON		60	*60

void Init_Timer(void *Task_1000ms);
uint32_t GetTimestamp(void);
uint16_t GetEventTimer_s(void);
void ClearEventTimer_s(void);
