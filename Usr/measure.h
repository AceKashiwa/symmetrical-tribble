#ifndef __MEASURE_H
#define __MEASURE_H

#include "stm32f10x.h"
#include "config.h"

typedef struct
{
    float freq_hz;
    float gain_db;
} FreqRespPoint;

typedef struct
{
    float rin_ohm;      /* 输入电阻 */
    float rout_ohm;     /* 输出电阻 */
    float gain_db;      /* 1kHz 增益 */
    float uout_1k_v;     /* 1kHz 时输出电压幅值信息（供故障判断用：接近饱和/截止的直流特征） */
    FreqRespPoint freqresp[FREQRESP_POINT_NUM];
    float cutoff_freq_hz; /* 上限截止频率（线性插值得到） */
    float lowfreq_gain_db; /* 20Hz 增益（供 C2 故障判断） */
    float lowfreq_phase_diff_rad; /* 20Hz 电压电流相位差（供 C1 故障判断） */
} MeasureResult;

/* 在 1kHz 测试频率下测量输入电阻 */
float Measure_InputResistance(void);

/* 两次测量法测量输出电阻（内部会切换继电器） */
float Measure_OutputResistance(void);

/* 1kHz 增益（dB） */
float Measure_Gain_dB(void);

/* 幅频特性全扫描 + 线性插值求上限截止频率，结果写入 out 数组(长度FREQRESP_POINT_NUM)
 * 返回上限截止频率(Hz) */
float Measure_FreqResponse(FreqRespPoint *out);

/* 20Hz 下的电压-电流相位差（用于C1开路/加倍判断），弧度 */
float Measure_LowFreqPhaseDiff(void);

/* 一次性完成全部参数测量，填充 MeasureResult */
void Measure_All(MeasureResult *result);

#endif
