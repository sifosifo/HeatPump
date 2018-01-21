#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "ComInterface.h"

typedef void (*Task_1_T)(void);

Task_1_T Task_1;
uint32_t Timestamp = 0;	// 10th of s (1Tick = 100ms)
uint16_t EventTimer_s = 0;

void Init_Timer(void *Task_1000ms)
{
	TCNT1 = 65535 - 62500;	
	TCCR1B = (1<<CS12);	// Fosc/256	
	TIMSK1 |= (1<<TOIE1);
	Task_1 = Task_1000ms;
}


// 16 000 000 / 256 = 62500
ISR(TIMER1_OVF_vect)
{
	Timestamp++;
	TCNT1 = 65535 - 62500;	// 1s
	//TCNT1 = 65535 - 6250;	// 100ms
//	SendBootupMessage(0xAA);
	(*Task_1)();
	EventTimer_s++;
}

uint32_t GetTimestamp(void)
{
	return(Timestamp);
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
	SendBootupMessage(0xBB);
//	(*Task_1)();
/*
	can_t msg;
	
	msg.id = 0x1;
	msg.flags.rtr = 0;
	msg.flags.extended = 0;	
	msg.length = 1;

	can_send_message(&msg);
	*/
}
