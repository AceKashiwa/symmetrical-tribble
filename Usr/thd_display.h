#ifndef __THD_DISPLAY_H
#define __THD_DISPLAY_H

#include "stm32f10x.h"
#include "bsp_ili9341_lcd.h"
#include "bsp_xpt2046_lcd.h"

/* ============================================================================
   数据来源约定
   ----------------------------------------------------------------------------
   本库只负责“显示”，不做 ADC/FFT/THD 计算。THD 项目里的 Calc_ProcessData()
   需要保持下面这两个 extern 变量是最新值：
     - ADC_AverageValue[128] : 最新一轮滤波后的 ADC 采样点（一个完整周期）
     - THD                   : THD 计算结果，取值范围 0.0~1.0 的小数
   如果你项目里这两个变量的名字/类型不同，改下面的 extern 声明即可。
   ============================================================================ */
#define THD_DISPLAY_NUM_SAMPLES   128
#define THD_DISPLAY_ADC_MAX       4095      /* 12位ADC满量程码值 */
#define THD_DISPLAY_VREF_MV       3300      /* ADC参考电压，单位mV，按实际板子调整 */

extern __IO uint16_t ADC_AverageValue[THD_DISPLAY_NUM_SAMPLES];
extern __IO float    THD;
extern __IO uint32_t FFT_Magnitude[65];  /* bins 0~64, 与main.c里的定义对应 */

/* ============================================================================
   失真类型选择 (PB12/PB13 -> CD4052)
   ----------------------------------------------------------------------------
   下面的映射只是占位("对应关系可随意确认")，等实际电路确定后，
   只需要改这里的枚举值和 THD_DISPLAY_BTN_LABELS，其它代码不用动。
   ============================================================================ */
typedef enum {
  THD_WAVE_SINE = 0,          /* 正弦波(无失真)，实测 PB12=1 PB13=0 PB15=1 */
  THD_WAVE_CROSSOVER,         /* 交越失真，        实测 PB12=1 PB13=0 PB15=0 */
  THD_WAVE_TOP_CLIP,          /* 顶部失真，        实测 PB12=0 PB13=1 PB15=1 */
  THD_WAVE_BOTTOM_CLIP,       /* 底部失真，        实测 PB12=1 PB13=1 PB15=1 */
  THD_WAVE_BIDIRECTIONAL,     /* 双边失真，        实测 PB12=0 PB13=0 PB15=1 */
} THD_WaveformType;

/* ============================================================================
   对外接口
   ============================================================================ */

/** 一次性初始化：配置 PB12/13/15 为输出，绘制静态UI布局。
 *  必须在 ILI9341_Init() / XPT2046_Init() / ILI9341_GramScan() 之后调用，
 *  因为需要用到 LCD_X_LENGTH/LCD_Y_LENGTH 来排版。 */
void THD_Display_Init(void);

/** 根据当前 ADC_AverageValue[] 重绘波形区域。
 *  必须在主循环上下文调用（不能在ISR里调用，LCD绘制比较耗时），
 *  建议每次 Calc_ProcessData() 跑完之后调用一次。 */
void THD_Display_UpdateWaveform(void);

/** 根据当前 THD/ADC_AverageValue 重绘 THD% 和关键电压值文字面板。
 *  和 THD_Display_UpdateWaveform() 配合使用。 */
void THD_Display_UpdateValues(void);

/** 根据当前 FFT_Magnitude[] 重绘频谱区域（对数刻度，相对最高峰值归一化）。
 *  和 THD_Display_UpdateWaveform()/UpdateValues() 一起在主循环里调用。 */
void THD_Display_UpdateSpectrum(void);

/** 触摸事件回调 —— 接入 bsp_xpt2046_lcd.c 里的
 *  XPT2046_TouchDown()/XPT2046_TouchUp()，替换掉里面对
 *  Touch_Button_Down/Up 的调用（那是画板demo专用的）。 */
void THD_Display_TouchDown(uint16_t x, uint16_t y);
void THD_Display_TouchUp(uint16_t x, uint16_t y);

/** 供其它模块查询当前选择状态 */
THD_WaveformType THD_Display_GetWaveformType(void);

#endif /* __THD_DISPLAY_H */