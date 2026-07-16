#include "fault_detect.h"
#include "config.h"
#include <math.h>

static MeasureResult s_baseline;
static uint8_t s_baseline_valid = 0;

void FaultDetect_SetBaseline(const MeasureResult *baseline) {
  s_baseline = *baseline;
  s_baseline_valid = 1;
}

static uint8_t Near(float v, float target, float tol) {
  return (fabsf(v - target) <= tol) ? 1 : 0;
}

/* 判断逻辑基于第1轮实测数据标定（见表1）：
 * 第一级：Vout 被拉到 VCC 附近 → R1开/R2短/R3短/R4开，用 Rin 大小细分
 * 第二级：Vout 被拉到特征电压 → R1短/R2开
 * 第三级：Vout 近零 → R3开/R4短，用 Rin 区分
 * 第四级：Vout 正常 → C1~C3 开/加倍，靠增益/相位/截止频率变化判断
 */
FaultType FaultDetect_Run(const MeasureResult *cur) {
  float rin = cur->rin_ohm;
  float vout = cur->uout_1k_v;

  if (!s_baseline_valid) {
    return FAULT_UNKNOWN;
  }

  /* ===== 第一级：Vout 被拉到 VCC 附近 (≥11V) ===== */
  if (vout >= FAULT_VOUT_VCC_THRESH) {
    if (rin <= FAULT_RIN_R2SHORT_MAX_OHM)
      return FAULT_R2_SHORT; /* 基极接地，Rin≈0 */
    if (rin >= FAULT_RIN_R1OPEN_MIN_OHM)
      return FAULT_R1_OPEN; /* Rin 升高最多（实测12~15k） */
    if (rin >= FAULT_RIN_R4OPEN_MIN_OHM)
      return FAULT_R4_OPEN; /* Rin 中等升高（实测7~11k） */
    return FAULT_R3_SHORT;  /* Rin正常，Vout被拉到VCC → R3短路 */
  }

  /* ===== 第二级：R1短路 & R2开路（特征钳位电压） ===== */
  if (Near(vout, FAULT_VOUT_R1SHORT_V, FAULT_VOUT_R1SHORT_TOL))
    return FAULT_R1_SHORT;
  if (Near(vout, FAULT_VOUT_R2OPEN_V, FAULT_VOUT_R2OPEN_TOL))
    return FAULT_R2_OPEN;

  /* ===== 第三级：Vout 近零（R3开路 / R4短路），用 Rin 区分 ===== */
  if (vout <= FAULT_VOUT_R3OPEN_MAX_V) {
    if (rin <= FAULT_RIN_R4SHORT_MAX_OHM)
      return FAULT_R4_SHORT; /* Rin很低 → 射极电阻短路到地 */
    return FAULT_R3_OPEN;    /* Rin正常 → 集电极开路，无电流 */
  }

  /* ===== 第四级：电容类故障（Vout 正常，靠增益/相位/截止频率变化判断） =====
   */

  /* C1 开路：输入断开，Rin→∞ 且无信号传递 */
  if (rin >= FAULT_RIN_C1OPEN_MIN_OHM &&
      cur->gain_db <= FAULT_GAIN_NEAR_ZERO_DB)
    return FAULT_C1_OPEN;

  /* C1 加倍：20Hz 电压-电流相位差明显偏移（耦合电容加倍改变低频相位） */
  if (fabsf(cur->lowfreq_phase_diff_rad - s_baseline.lowfreq_phase_diff_rad) >
      FAULT_PHASE_DIFF_TOL_RAD)
    return FAULT_C1_DOUBLE;

  /* C2 开路：Rin增大且1kHz增益显著下降（实测从124→2.93，降约32dB） */
  if (rin > s_baseline.rin_ohm * 2.0f &&
      (s_baseline.gain_db - cur->gain_db) >= FAULT_GAIN_DROP_DB)
    return FAULT_C2_OPEN;

  /* C2 加倍：20Hz 低频增益明显升高（实测上升约10~12dB） */
  if ((cur->lowfreq_gain_db - s_baseline.lowfreq_gain_db) >= FAULT_GAIN_RISE_DB)
    return FAULT_C2_DOUBLE;

  /* C3 开路：截止频率明显升高（>10%偏移） */
  if ((cur->cutoff_freq_hz - s_baseline.cutoff_freq_hz) >
      s_baseline.cutoff_freq_hz * FAULT_CUTOFF_SHIFT_RATIO)
    return FAULT_C3_OPEN;

  /* C3 加倍：截止频率明显降低（>10%偏移） */
  if ((s_baseline.cutoff_freq_hz - cur->cutoff_freq_hz) >
      s_baseline.cutoff_freq_hz * FAULT_CUTOFF_SHIFT_RATIO)
    return FAULT_C3_DOUBLE;

  /* 各项指标均在正常范围内 */
  return FAULT_NONE;
}

const char *FaultDetect_TypeName(FaultType type) {
  switch (type) {
  case FAULT_NONE:
    return "NORMAL";
  case FAULT_R1_OPEN:
    return "R1_OPEN";
  case FAULT_R1_SHORT:
    return "R1_SHORT";
  case FAULT_R2_OPEN:
    return "R2_OPEN";
  case FAULT_R2_SHORT:
    return "R2_SHORT";
  case FAULT_R3_OPEN:
    return "R3_OPEN";
  case FAULT_R3_SHORT:
    return "R3_SHORT";
  case FAULT_R4_OPEN:
    return "R4_OPEN";
  case FAULT_R4_SHORT:
    return "R4_SHORT";
  case FAULT_C1_OPEN:
    return "C1_OPEN";
  case FAULT_C1_DOUBLE:
    return "C1_DOUBLE";
  case FAULT_C2_OPEN:
    return "C2_OPEN";
  case FAULT_C2_DOUBLE:
    return "C2_DOUBLE";
  case FAULT_C3_OPEN:
    return "C3_OPEN";
  case FAULT_C3_DOUBLE:
    return "C3_DOUBLE";
  default:
    return "UNKNOWN";
  }
}
