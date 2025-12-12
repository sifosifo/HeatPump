
#define CYCLING_PROTECTION_PERIOD_OFF	15      //6*60
#define CYCLING_PROTECTION_PERIOD_ON	10*60
#define COMPRESSOR_COOLDOWN_PERIOD      5
#define FLOW_CHECKING_TIMEOUT_PERIOD	10
#define	RECOVERABLE_ERROR_PERIOD_ON		60	*60

void timer_Init(void *Task_1000ms);
uint32_t timer_GetTimestamp_s(void);
void timer_Tick(void);
uint16_t GetEventTimer_s(void);
void ClearEventTimer_s(void);

void timer_start_ms(uint8_t index);
void timer_stop_ms(uint8_t index);
uint16_t timer_get_ms(uint8_t index);