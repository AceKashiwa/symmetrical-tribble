#ifndef __ADC_SAMPLE_H
#define __ADC_SAMPLE_H

#include "stm32f10x.h"

/* 通道在DMA缓冲区中的顺序索引（与 config.h 中 ADC_CH_xxx 的注入顺序一致） */
#define ADC_IDX_UIN   0
#define ADC_IDX_IIN   1
#define ADC_IDX_UOUT  2
#define ADC_CH_NUM    3

void ADC_Sample_Init(void);

/* 以指定采样率启动一次单发采样（阻塞等待采集完成），采集
 * ADC_SAMPLE_POINTS 组（每组3通道），采集完成后数据保存在内部缓冲区 */
void ADC_Sample_Once(uint32_t sample_rate_hz);

/* 获取内部缓冲区中某一通道的采样数据指针和点数 */
void ADC_Sample_GetChannel(uint8_t ch_idx, uint16_t **buf, uint32_t *count);

/* 计算某通道的峰峰值电压（V），基于最近一次 ADC_Sample_Once 的数据 */
float ADC_Sample_GetVpp(uint8_t ch_idx);

/* 计算某通道的直流平均值电压（V） */
float ADC_Sample_GetVavg(uint8_t ch_idx);

#endif
