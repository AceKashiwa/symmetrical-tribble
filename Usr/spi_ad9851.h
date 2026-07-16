#ifndef __SPI_AD9851_H
#define __SPI_AD9851_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f10x.h"

#define AD9851_FQ_UD_PORT GPIOA
#define AD9851_FQ_UD_PIN GPIO_Pin_4

#define AD9851_W_CLK_PORT GPIOA
#define AD9851_W_CLK_PIN GPIO_Pin_5

#define AD9851_RESET_PORT GPIOA
#define AD9851_RESET_PIN GPIO_Pin_6

#define AD9851_D7_PORT GPIOA
#define AD9851_D7_PIN GPIO_Pin_7

#define AD9851_REF_CLK_HZ 180000000UL
#define AD9851_USE_REFCLK_MULTIPLIER 1

void SPI_AD9851_Init(void);
void AD9851_Reset(void);
void AD9851_SetFrequency(u32 freq_hz);
void AD9851_SetFrequencyPhase(u32 freq_hz, u8 phase_5bit);
void AD9851_PowerDown(void);
void AD9851_PowerUp(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_AD9851_H */
