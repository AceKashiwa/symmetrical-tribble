#ifndef __BSP_PANEL_H
#define __BSP_PANEL_H

#include "stm32f10x.h"
#include "measure.h"

/******************************* 工作模式枚举 ***************************/
typedef enum
{
    MODE_MEASURE = 0,       /* Mode1: 测量输入/输出电阻、1kHz增益、幅频特性 */
    MODE_FAULT              /* Mode2: 自动检测电路故障并给出原因 */
} PanelMode_t;

/******************************* 触摸触发的动作枚举 ***************************/
typedef enum
{
    ACTION_NONE = 0,
    ACTION_MEASURE_ALL,         /* 全部测量 */
    ACTION_INPUT_R,             /* 输入电阻 */
    ACTION_OUTPUT_R,            /* 输出电阻 */
    ACTION_GAIN_1K,             /* 1kHz增益 */
    ACTION_FREQ_RESP,           /* 幅频特性(含绘图) */
    ACTION_FAULT_DETECT,        /* 自动故障检测 */
} PanelAction_t;

/******************************* 面板函数声明 ***************************/
void          Panel_Init                  (void);
void          Panel_HandleTouch           (uint16_t usX, uint16_t usY);
void          Panel_SetMode               (PanelMode_t mode);
PanelMode_t   Panel_GetMode               (void);

uint8_t       Panel_IsNewAction           (void);
PanelAction_t Panel_FetchAction           (void);

/* 结果显示区（右侧区域），最多支持的行数 */
#define PANEL_RESULT_MAX_LINES   8
void Panel_ShowResult(const char * const *lines, uint8_t line_count);

/* ---- Debug 面板：在右侧底部显示中间信号用于测试代码可行性 ---- */
void Panel_Debug_Clear(void);
void Panel_Debug_ShowFloat(const char *label, float value, const char *unit);
void Panel_Debug_ShowInt(const char *label, int32_t value, const char *unit);

/* ---- 幅频特性曲线绘制（在右侧绘图区绘制） ---- */
void Panel_DrawFreqResp(const FreqRespPoint *points, uint8_t count,
                        float mid_gain_db, float cutoff_freq);

#endif /* __BSP_PANEL_H */