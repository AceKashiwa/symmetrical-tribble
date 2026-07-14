#include "fault_detect.h"
#include "config.h"
#include <math.h>

static MeasureResult s_baseline;
static uint8_t s_baseline_valid = 0;

void FaultDetect_SetBaseline(const MeasureResult *baseline)
{
    s_baseline = *baseline;
    s_baseline_valid = 1;
}

static uint8_t Near(float v, float target, float tol)
{
    return (fabsf(v - target) <= tol) ? 1 : 0;
}

/* 判断逻辑对应报告 2.2 节 (1)~(7) 七类故障的理论特征：
 * 输出电压是否被钳位到某个特征值 + 输入电阻是否异常大/异常小，
 * 是分辨 R1~R4 各种开路/短路的主要依据；
 * C1~C3 的变化则主要看低频/高频增益变化和低频相位差变化。
 * 每一类的具体数值都来自报告中"电阻/电容变化"章节的理论分析，
 * 由于和实际焊接的电路参数(β、静态工作点等)有关，请用你自己的电路
 * 把每种故障人为制造出来后，对照实测数据把 config.h 里的阈值改准确。
 */
FaultType FaultDetect_Run(const MeasureResult *cur)
{
    float rin = cur->rin_ohm;
    float vout = cur->uout_1k_v;

    if (!s_baseline_valid)
    {
        return FAULT_UNKNOWN;
    }

    /* ---------- 输出被钳位到 VCC 附近的几种情况：R1开路 / R2短路 / R3短路 / R4开路 ---------- */
    if (Near(vout, VCC_V, FAULT_VOUT_VCC_TOL))
    {
        if (rin <= FAULT_RIN_SHORT_MAX_OHM)
        {
            return FAULT_R2_SHORT; /* 基极接地，三极管截止，Rin≈0 */
        }
        if (rin >= FAULT_RIN_OPEN_MIN_OHM)
        {
            return FAULT_R1_OPEN; /* Rin 很大，且明显大于 R4开路 的门限 */
        }
        if (rin >= FAULT_RIN_R4OPEN_MAX_OHM)
        {
            return FAULT_R4_OPEN; /* Rin 也很大，但略小于 R1 开路 */
        }
        /* Rin 接近基准正常值 -> 输出直接被钳到12V，但输入回路未受影响 */
        return FAULT_R3_SHORT;
    }

    /* ---------- 其余被钳位到特征电压的情况 ---------- */
    if (Near(vout, FAULT_VOUT_R1SHORT_V, FAULT_VOUT_R1SHORT_TOL))
    {
        return FAULT_R1_SHORT;
    }
    if (Near(vout, FAULT_VOUT_R2OPEN_V, FAULT_VOUT_R2OPEN_TOL))
    {
        return FAULT_R2_OPEN;
    }
    if (Near(vout, FAULT_VOUT_R3OPEN_V, FAULT_VOUT_R3OPEN_TOL))
    {
        return FAULT_R3_OPEN;
    }
    if (vout <= FAULT_VOUT_R4SHORT_MAX_V && rin <= FAULT_RIN_SHORT_MAX_OHM)
    {
        return FAULT_R4_SHORT;
    }

    /* ---------- 电容类故障：输出电压仍在正常范围内，靠增益/相位变化判断 ---------- */

    /* C1 开路：输入端口断开，1kHz 处基本测不到信号传递，但静态工作点(Vout直流)未偏移 */
    if (rin >= FAULT_RIN_OPEN_MIN_OHM && cur->gain_db <= FAULT_GAIN_NEAR_ZERO_DB)
    {
        return FAULT_C1_OPEN;
    }

    /* C1 加倍：低频(20Hz)电压-电流相位差相对基准发生明显变化 */
    if (fabsf(cur->lowfreq_phase_diff_rad - s_baseline.lowfreq_phase_diff_rad) >
        FAULT_PHASE_DIFF_TOL_RAD)
    {
        return FAULT_C1_DOUBLE;
    }

    /* C2 开路：输入电阻变大，且1kHz增益相对基准明显降低 */
    if (rin > s_baseline.rin_ohm * 2.0f &&
        (s_baseline.gain_db - cur->gain_db) >= FAULT_GAIN_DROP_DB)
    {
        return FAULT_C2_OPEN;
    }

    /* C2 加倍：低频增益相对基准明显升高 */
    if ((cur->lowfreq_gain_db - s_baseline.lowfreq_gain_db) >= FAULT_GAIN_RISE_DB)
    {
        return FAULT_C2_DOUBLE;
    }

    /* C3 开路：高频（截止频率附近）增益相对基准升高，或截止频率明显升高 */
    if ((cur->cutoff_freq_hz - s_baseline.cutoff_freq_hz) >
        s_baseline.cutoff_freq_hz * 0.1f)
    {
        return FAULT_C3_OPEN;
    }

    /* C3 加倍：截止频率明显降低 */
    if ((s_baseline.cutoff_freq_hz - cur->cutoff_freq_hz) >
        s_baseline.cutoff_freq_hz * 0.1f)
    {
        return FAULT_C3_DOUBLE;
    }

    /* 与基准相比各项指标均在正常范围内 */
    return FAULT_NONE;
}

const char *FaultDetect_TypeName(FaultType type)
{
    switch (type)
    {
        case FAULT_NONE:      return "NORMAL";
        case FAULT_R1_OPEN:   return "R1_OPEN";
        case FAULT_R1_SHORT:  return "R1_SHORT";
        case FAULT_R2_OPEN:   return "R2_OPEN";
        case FAULT_R2_SHORT:  return "R2_SHORT";
        case FAULT_R3_OPEN:   return "R3_OPEN";
        case FAULT_R3_SHORT:  return "R3_SHORT";
        case FAULT_R4_OPEN:   return "R4_OPEN";
        case FAULT_R4_SHORT:  return "R4_SHORT";
        case FAULT_C1_OPEN:   return "C1_OPEN";
        case FAULT_C1_DOUBLE: return "C1_DOUBLE";
        case FAULT_C2_OPEN:   return "C2_OPEN";
        case FAULT_C2_DOUBLE: return "C2_DOUBLE";
        case FAULT_C3_OPEN:   return "C3_OPEN";
        case FAULT_C3_DOUBLE: return "C3_DOUBLE";
        default:              return "UNKNOWN";
    }
}
