#include "measure.h"
#include "ad9851.h"
#include "adc_sample.h"
#include "relay.h"
#include "goertzel.h"
#include "systick_delay.h"
#include <math.h>

#define SETTLE_MS   20   /* 切换频率/负载后，等待电路建立稳定的延时 */
#define I_EPS_AMP   1e-9f

float Measure_InputResistance(void)
{
    float uin_meas_pp, iin_meas_pp;
    float uin_actual_pp, iin_actual_amp;

    AD9851_SetFrequency(TEST_FREQ_HZ);
    Delay_ms(SETTLE_MS);
    ADC_Sample_Once(ADC_SAMPLE_RATE_HZ_DEFAULT);

    uin_meas_pp = ADC_Sample_GetVpp(ADC_IDX_UIN);
    iin_meas_pp = ADC_Sample_GetVpp(ADC_IDX_IIN);

    uin_actual_pp = uin_meas_pp;
    /* V_meas(pp) = Iin(pp) * R_SENSE * AD620_GAIN */
    iin_actual_amp = iin_meas_pp / (R_SENSE_OHM * AD620_GAIN);

    if (iin_actual_amp < I_EPS_AMP)
    {
        return 1.0e9f; /* 视为开路，输入电阻趋于无穷大 */
    }
    return uin_actual_pp / iin_actual_amp;
}

float Measure_OutputResistance(void)
{
    float uout_noload_pp, uout_load_pp;
    float uout_noload, uout_load;

    AD9851_SetFrequency(TEST_FREQ_HZ);

    Relay_SetLoad(0); /* 空载 */
    Delay_ms(SETTLE_MS);
    ADC_Sample_Once(ADC_SAMPLE_RATE_HZ_DEFAULT);
    uout_noload_pp = ADC_Sample_GetVpp(ADC_IDX_UOUT);

    Relay_SetLoad(1); /* 接入 2k 已知负载 */
    Delay_ms(SETTLE_MS);
    ADC_Sample_Once(ADC_SAMPLE_RATE_HZ_DEFAULT);
    uout_load_pp = ADC_Sample_GetVpp(ADC_IDX_UOUT);

    Relay_SetLoad(0); /* 恢复空载，避免影响后续测量 */

    uout_noload = uout_noload_pp ;
    uout_load = uout_load_pp;

    if (uout_load < 1e-6f)
    {
        return 1.0e9f;
    }
    /* Rout = R_LOAD * (Uout_空载 - Uout_带载) / Uout_带载 */
    return R_LOAD_OHM * (uout_noload - uout_load) / uout_load;
}

float Measure_Gain_dB(void)
{
    float uin_pp, uout_pp;

    AD9851_SetFrequency(TEST_FREQ_HZ);
    Delay_ms(SETTLE_MS);
    ADC_Sample_Once(ADC_SAMPLE_RATE_HZ_DEFAULT);

    uin_pp = ADC_Sample_GetVpp(ADC_IDX_UIN);
    uout_pp = ADC_Sample_GetVpp(ADC_IDX_UOUT);

    if (uin_pp < 1e-6f)
    {
        return 0.0f;
    }
    return 20.0f * log10f(uout_pp / uin_pp);
}

/* 根据某一频率点计算增益(dB)，内部复用 Measure_Gain_dB 的采样方式 */
static float MeasureGainAtFreq(uint32_t freq_hz, uint32_t sample_rate_hz)
{
    float uin_pp, uout_pp;

    AD9851_SetFrequency(freq_hz);
    Delay_ms(SETTLE_MS);
    ADC_Sample_Once(sample_rate_hz);

    uin_pp = ADC_Sample_GetVpp(ADC_IDX_UIN);
    uout_pp = ADC_Sample_GetVpp(ADC_IDX_UOUT);

    if (uin_pp < 1e-6f)
    {
        return -200.0f; /* 异常值，避免除0 */
    }
    return 20.0f * log10f(uout_pp / uin_pp);
}

/* 幅频特性测量：按报告4.2.1，逐个十倍频点测量增益，
 * 找到中频段增益作为参考（取1kHz点），再用相邻两点在
 * (log10(f), gain_dB) 空间做线性插值，求 -3dB 转折频率所在区间
 * 及其插值频率点 */
float Measure_FreqResponse(FreqRespPoint *out)
{
    int i;
    float mid_gain_db = 0.0f;
    float cutoff_freq = 0.0f;
    uint8_t cutoff_found = 0;

    for (i = 0; i < FREQRESP_POINT_NUM; i++)
    {
        uint32_t f = FREQRESP_POINTS_HZ[i];
        uint32_t fs = (f <= 100) ? ADC_SAMPLE_RATE_HZ_LOWFREQ
                                  : ADC_SAMPLE_RATE_HZ_DEFAULT;
        out[i].freq_hz = (float)f;
        out[i].gain_db = MeasureGainAtFreq(f, fs);

        if (f == TEST_FREQ_HZ)
        {
            mid_gain_db = out[i].gain_db;
        }
    }

    /* 从中频往高频方向找 -3dB 点（本设计主要关心上限截止频率，
     * 若需要同时找下限截止频率，可对低频方向做同样处理） */
    for (i = 0; i < FREQRESP_POINT_NUM - 1; i++)
    {
        float g1 = out[i].gain_db;
        float g2 = out[i + 1].gain_db;
        float target = mid_gain_db - 3.0f;

        if ((g1 >= target && g2 < target) || (g1 <= target && g2 > target))
        {
            /* 在 log10(f) - gain_dB 空间线性插值 */
            float lf1 = log10f(out[i].freq_hz);
            float lf2 = log10f(out[i + 1].freq_hz);
            float t = (target - g1) / (g2 - g1);
            float lf = lf1 + t * (lf2 - lf1);
            cutoff_freq = powf(10.0f, lf);
            cutoff_found = 1;
            break;
        }
    }

    if (!cutoff_found)
    {
        /* 未在扫描范围内找到 -3dB 点，返回扫描的最高频率作为近似上限 */
        cutoff_freq = out[FREQRESP_POINT_NUM - 1].freq_hz;
    }

    return cutoff_freq;
}

float Measure_LowFreqPhaseDiff(void)
{
    uint16_t *buf_u, *buf_i;
    uint32_t cnt_u, cnt_i;
    float mag_u, phase_u, mag_i, phase_i;

    AD9851_SetFrequency(LOWFREQ_TEST_HZ);
    Delay_ms(SETTLE_MS + 30); /* 低频建立时间稍长 */
    ADC_Sample_Once(ADC_SAMPLE_RATE_HZ_LOWFREQ);

    ADC_Sample_GetChannel(ADC_IDX_UIN, &buf_u, &cnt_u);
    ADC_Sample_GetChannel(ADC_IDX_IIN, &buf_i, &cnt_i);

    Goertzel_Detect(buf_u, cnt_u, (float)LOWFREQ_TEST_HZ,
                    (float)ADC_SAMPLE_RATE_HZ_LOWFREQ, &mag_u, &phase_u);
    Goertzel_Detect(buf_i, cnt_i, (float)LOWFREQ_TEST_HZ,
                    (float)ADC_SAMPLE_RATE_HZ_LOWFREQ, &mag_i, &phase_i);

    (void)mag_u;
    (void)mag_i;

    return phase_u - phase_i;
}

void Measure_All(MeasureResult *result)
{
    result->rin_ohm = Measure_InputResistance();
    result->rout_ohm = Measure_OutputResistance();
    result->gain_db = Measure_Gain_dB();

    AD9851_SetFrequency(TEST_FREQ_HZ);
    Delay_ms(SETTLE_MS);
    ADC_Sample_Once(ADC_SAMPLE_RATE_HZ_DEFAULT);
    result->uout_1k_v = ADC_Sample_GetVavg(ADC_IDX_UOUT);

    result->cutoff_freq_hz = Measure_FreqResponse(result->freqresp);
    result->lowfreq_gain_db = result->freqresp[0].gain_db; /* 数组第0项固定为20Hz */
    result->lowfreq_phase_diff_rad = Measure_LowFreqPhaseDiff();
}
