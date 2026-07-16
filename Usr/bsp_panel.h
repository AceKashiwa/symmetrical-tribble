#ifndef __BSP_PANEL_H
#define __BSP_PANEL_H

#include "stm32f10x.h"

/******************************* 测量项目枚举 ***************************/
typedef enum
{
    MEAS_NONE = 0,
    MEAS_INPUT_R,          /* 输入电阻 */
    MEAS_OUTPUT_R,         /* 输出电阻 */
    MEAS_GAIN,             /* 电压增益 */
    MEAS_FREQ_RESP,        /* 幅频特性 */
    MEAS_FAULT_R1,         /* 故障检测 R1 */
    MEAS_FAULT_R2,         /* 故障检测 R2 */
    MEAS_FAULT_R3,         /* 故障检测 R3 */
    MEAS_FAULT_R4,         /* 故障检测 R4 */
    MEAS_FAULT_C1,         /* 故障检测 C1 */
    MEAS_FAULT_C2,         /* 故障检测 C2 */
    MEAS_FAULT_C3,         /* 故障检测 C3 */
} MEAS_ITEM_t;

/******************************** 控制面板函数声明 **********************************/
void         Panel_Init                   ( void );
void         Panel_HandleTouch            ( uint16_t usX, uint16_t usY );
MEAS_ITEM_t  Panel_GetSelectedItem        ( void );
uint8_t      Panel_IsNewSelectionAvailable( void );
MEAS_ITEM_t  Panel_FetchSelection         ( void );
void         Panel_ClearSelection         ( void );

#endif /* __BSP_PANEL_H */