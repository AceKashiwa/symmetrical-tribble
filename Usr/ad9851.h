#ifndef __AD9851_H
#define __AD9851_H

#include "stm32f10x.h"

void AD9851_GPIO_Init(void);
void AD9851_Reset(void);
void AD9851_SetFrequency(uint32_t freq_hz);

#endif
