#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f10x.h"

void Relay_Init(void);
void Relay_SetLoad(uint8_t on); /* on=1 接入2k负载，on=0 断开(空载) */

#endif
