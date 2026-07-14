#include "rcc.h"

void RCC_Configuration(void) {
  RCC_ADCCLKConfig(RCC_PCLK2_Div8);

  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOA |
                             RCC_APB2Periph_GPIOB | RCC_APB2Periph_ADC1 |
                             RCC_APB2Periph_TIM1 | RCC_APB2Periph_USART1,
                         ENABLE);
}