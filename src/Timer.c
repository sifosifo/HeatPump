#include <stdint.h>
#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

typedef void (*Task_1_T)(void);

Task_1_T Task_1;
uint32_t Timestamp_s = 0;	// uptime
uint16_t EventTimer_s = 0;

uint16_t Timestamp_ms[3] = {0, 0, 0};
uint16_t Timestamp_max_ms[3] = {0, 0, 0};

void timer_Init(void *Task_1000ms)
{	// Timer 0 already used for _delay_ms()
	// Initialize Timer1 for 1s interrupts
	TCNT1 = 65535 - 62500;	
	TCCR1B = (1<<CS12);	// Fosc/256	
	TIMSK1 |= (1<<TOIE1);
	Task_1 = Task_1000ms;

	// Initialize Timer2 for 1ms interrupts
   	TCCR2A = (1<<WGM21);                // CTC
    TCCR2B = (1<<CS22);      // prescaler 32
    // 16MHz / 64 = 250 kHz → 250 ticks = 1 ms → OCR2A = 249
    OCR2A = 249;
    TIMSK2 = (1<<OCIE2A);
}

ISR(TIMER2_COMPA_vect)
{
    if(Timestamp_ms[0] < 0xFFFF)	Timestamp_ms[0]++;	// prevent overflow
	if(Timestamp_ms[1] < 0xFFFF)	Timestamp_ms[1]++;	// prevent overflow
	if(Timestamp_ms[2] < 0xFFFF)	Timestamp_ms[2]++;	// prevent overflow
}

// 16 000 000 / 256 = 62500
ISR(TIMER1_OVF_vect)
{
	Timestamp_s++;
	TCNT1 = 65535 - 62500;	// 1s
	//TCNT1 = 65535 - 6250;	// 100ms
	(*Task_1)();
	EventTimer_s++;
}

void timer_start_ms(uint8_t index)
{
	Timestamp_ms[index] = 0;
}

void timer_stop_ms(uint8_t index)
{
	if(Timestamp_ms[index] > Timestamp_max_ms[index])
	{
		Timestamp_max_ms[index] = Timestamp_ms[index];
	}
}

uint16_t timer_get_ms(uint8_t index)
{
	return(Timestamp_max_ms[index]);
}

uint32_t timer_GetTimestamp_s(void)
{
	return(Timestamp_s);
}

uint16_t GetEventTimer_s(void)
{
	return(EventTimer_s);
}

void ClearEventTimer_s(void)
{
	EventTimer_s = 0;
}

ISR(__vector_default)
{
	TCNT1 = 65535 - 62500;
}
