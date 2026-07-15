#ifndef __SYSTICK_DELAY_H
#define __SYSTICK_DELAY_H

#include "stm32f10x.h"

extern volatile uint32_t g_systick_ms;

void SysTick_DelayInit(void);
void Delay_ms(uint32_t ms);

#endif
