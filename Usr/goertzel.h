#ifndef __GOERTZEL_H
#define __GOERTZEL_H

#include "stm32f10x.h"

/* 对采样序列做单频点检测（等效于该频率点上的DFT），
 * 比对整段做FFT更省资源，适合本设计"仅需知道某一固定频率处
 * 幅值和相位"的场景（报告4.2.2, 故障5电容C1的相位判断）。
 * samples:   采样数据（ADC原始码值即可，直流分量不影响相位/不影响幅值比较）
 * n:         采样点数
 * target_hz: 目标频率
 * fs_hz:     采样率
 * mag:       输出：该频率分量幅值（相对值，用于两通道比较足够）
 * phase_rad: 输出：该频率分量相位（弧度）
 */
void Goertzel_Detect(const uint16_t *samples, uint32_t n, float target_hz,
                     float fs_hz, float *mag, float *phase_rad);

#endif
