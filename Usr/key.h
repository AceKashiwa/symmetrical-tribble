#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

void Key_Init(void);

/* 返回并清除"模式切换请求"标志，由 EXTI 中断置位 */
uint8_t Key_GetSwitchRequest(void);

#endif
