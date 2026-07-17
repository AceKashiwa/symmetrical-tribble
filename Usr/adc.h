#ifndef __ADC_H
#define	__ADC_H


#include "stm32f10x.h"

// ====== 双ADC 3通道规则同步 + 欠采样相位测量 ======
// 采样率: ~285.7 kHz/通道 (12MHz ADC时钟, 1.5周期采样, 3通道扫描)
// 策略: 已知信号频率(AD9851生成), 用欠采样+正弦拟合测相位差
//       配频技巧: f_DDS / f_sample = 有理数 → 等效时间采样
//       例: 200kHz / 285.7kHz = 7/10 → 10个唯一样点分布7个周期

// ---------- ADC1（主）----------
#define    ADCx_1                           ADC1
#define    ADCx_1_APBxClock_FUN             RCC_APB2PeriphClockCmd
#define    ADCx_1_CLK                       RCC_APB2Periph_ADC1

#define    ADCx_1_GPIO_APBxClock_FUN        RCC_APB2PeriphClockCmd
#define    ADCx_1_GPIO_CLK                  RCC_APB2Periph_GPIOC
#define    ADCx_1_PORT                      GPIOC

// ADC1 三通道: PC0/CH10, PC1/CH11, PC2/CH12
#define    ADC1_CH1_PIN                     GPIO_Pin_0
#define    ADC1_CH1_CHANNEL                 ADC_Channel_10
#define    ADC1_CH2_PIN                     GPIO_Pin_1
#define    ADC1_CH2_CHANNEL                 ADC_Channel_11
#define    ADC1_CH3_PIN                     GPIO_Pin_2
#define    ADC1_CH3_CHANNEL                 ADC_Channel_12

// ---------- ADC2（从）----------
#define    ADCx_2                           ADC2
#define    ADCx_2_APBxClock_FUN             RCC_APB2PeriphClockCmd
#define    ADCx_2_CLK                       RCC_APB2Periph_ADC2

#define    ADCx_2_GPIO_APBxClock_FUN        RCC_APB2PeriphClockCmd
#define    ADCx_2_GPIO_CLK                  RCC_APB2Periph_GPIOC
#define    ADCx_2_PORT                      GPIOC

// ADC2 三通道: PC3/CH13, PC4/CH14, PC5/CH15
#define    ADC2_CH1_PIN                     GPIO_Pin_3
#define    ADC2_CH1_CHANNEL                 ADC_Channel_13
#define    ADC2_CH2_PIN                     GPIO_Pin_4
#define    ADC2_CH2_CHANNEL                 ADC_Channel_14
#define    ADC2_CH3_PIN                     GPIO_Pin_5
#define    ADC2_CH3_CHANNEL                 ADC_Channel_15

// ---------- 参数 ----------
#define    NOFCHANEL                        3
#define    ADC_FSAMPLE                      285714UL  // Hz (实际采样率/通道)

// ---------- DMA ----------
#define    ADC_DMA_CHANNEL                  DMA1_Channel1
#define    ADC_DMA_BUF_SIZE                 (NOFCHANEL * 256)  // 768 个 32 位字

extern __IO uint32_t ADC_ConvertedValue[ADC_DMA_BUF_SIZE];
extern __IO uint8_t  ADC_BufferReady;

// 简化提取宏
#define ADC1_VAL(idx)  ((uint16_t)(ADC_ConvertedValue[idx] & 0xFFFF))
#define ADC2_VAL(idx)  ((uint16_t)(ADC_ConvertedValue[idx] >> 16))

void ADCx_Init(void);

#endif /* __ADC_H */

