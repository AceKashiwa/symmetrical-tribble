/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : 双模式测量系统主程序
 *                   Mode1 - 测量模式: 输入/输出电阻, 1kHz增益, 幅频特性曲线
 *                   Mode2 - 故障检测: 自动检测电路故障并给出原因
 *                   物理按键(KEY)用于两个模式间切换
 ******************************************************************************
 */

#include "stm32f10x.h"
#include "config.h"
#include "ad9851.h"
#include "adc_sample.h"
#include "relay.h"
#include "key.h"
#include "measure.h"
#include "fault_detect.h"
#include "systick_delay.h"
#include "usart_debug.h"
#include <stdio.h>
#include <string.h>
#include "bsp_ili9341_lcd.h"
#include "bsp_xpt2046_lcd.h"
#include "bsp_panel.h"

/* ======================== 辅助函数 ======================== */

/* UART 打印测量结果 */
static void PrintMeasureResult(const MeasureResult *r)
{
    int i;
    printf("---- Measure Result ----\r\n");
    printf("Rin  = %.2f ohm\r\n", r->rin_ohm);
    printf("Rout = %.2f ohm\r\n", r->rout_ohm);
    printf("Gain(1kHz) = %.2f dB\r\n", r->gain_db);
    printf("Uout(1kHz) = %.3f V\r\n", r->uout_1k_v);
    printf("Cutoff = %.1f Hz\r\n", r->cutoff_freq_hz);
    printf("LowFreqGain = %.2f dB\r\n", r->lowfreq_gain_db);
    printf("LowFreqPhaseDiff = %.3f rad\r\n", r->lowfreq_phase_diff_rad);
    printf("FreqResp:\r\n");
    for (i = 0; i < FREQRESP_POINT_NUM; i++)
    {
        printf("  f=%.0fHz  gain=%.2fdB\r\n",
               r->freqresp[i].freq_hz, r->freqresp[i].gain_db);
    }
}

/* 测量完成后的统一 LCD 显示 */
static void DisplayResults(const MeasureResult *r)
{
    static char lbuf[4][40];
    const char *lines[4];
    uint16_t vmin, vmax;

    /* 左列4行：核心测量结果 */
    snprintf(lbuf[0], sizeof(lbuf[0]), "Rin  = %.1f ohm", r->rin_ohm);
    snprintf(lbuf[1], sizeof(lbuf[1]), "Rout = %.1f ohm", r->rout_ohm);
    snprintf(lbuf[2], sizeof(lbuf[2]), "Gain = %.2f dB", r->gain_db);
    snprintf(lbuf[3], sizeof(lbuf[3]), "Fc   = %.0f Hz", r->cutoff_freq_hz);
    for (int i = 0; i < 4; i++) lines[i] = lbuf[i];
    Panel_ShowResult(lines, 4);

    /* 右列4行：补充数据 + Debug 原始值 */
    Panel_Debug_Clear();
    Panel_Debug_ShowFloat("VoutDC", r->uout_1k_v, "V");
    ADC_Sample_GetRawMinMax(ADC_IDX_UIN,  &vmin, &vmax);
    Panel_Debug_ShowInt("Uinpp",  (int32_t)(vmax - vmin), "raw");
    ADC_Sample_GetRawMinMax(ADC_IDX_IIN,  &vmin, &vmax);
    Panel_Debug_ShowInt("Iinpp",  (int32_t)(vmax - vmin), "raw");
    ADC_Sample_GetRawMinMax(ADC_IDX_UOUT, &vmin, &vmax);
    Panel_Debug_ShowInt("Uoutpp", (int32_t)(vmax - vmin), "raw");

    /* 幅频特性曲线 */
    Panel_DrawFreqResp(r->freqresp, FREQRESP_POINT_NUM,
                       r->gain_db, r->cutoff_freq_hz);
}

/* 统一的测量执行 + 串口输出 + LCD显示 */
static void RunMeasureAndDisplay(void)
{
    MeasureResult r;
    Panel_Debug_Clear();

    Measure_All(&r);
    PrintMeasureResult(&r);
    ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);
    DisplayResults(&r);
}

/* 故障检测执行 + 显示 */
static void RunFaultDetectAndDisplay(void)
{
    MeasureResult cur;

    Panel_Debug_Clear();

    Measure_All(&cur);
    PrintMeasureResult(&cur);
    ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);

    FaultType ft = FaultDetect_Run(&cur);
    const char *ftName = FaultDetect_TypeName(ft);
    printf("---- Fault Detect ---- Result: %s\r\n", ftName);

    /* 显示核心测量结果 + 幅频曲线 */
    DisplayResults(&cur);

    /* 叠加故障结果到左列第4行（覆盖 Fc 值），并加第5行描述 */
    {
        static char fl1[40], fl2[40];
        const char *flines[2];
        snprintf(fl1, sizeof(fl1), "Fault=%s", ftName);
        switch (ft) {
            case FAULT_NONE:      snprintf(fl2, sizeof(fl2), "Circuit NORMAL"); break;
            case FAULT_R1_OPEN:   snprintf(fl2, sizeof(fl2), "R1 open"); break;
            case FAULT_R1_SHORT:  snprintf(fl2, sizeof(fl2), "R1 short"); break;
            case FAULT_R2_OPEN:   snprintf(fl2, sizeof(fl2), "R2 open"); break;
            case FAULT_R2_SHORT:  snprintf(fl2, sizeof(fl2), "R2 short"); break;
            case FAULT_R3_OPEN:   snprintf(fl2, sizeof(fl2), "R3 open"); break;
            case FAULT_R3_SHORT:  snprintf(fl2, sizeof(fl2), "R3 short"); break;
            case FAULT_R4_OPEN:   snprintf(fl2, sizeof(fl2), "R4 open"); break;
            case FAULT_R4_SHORT:  snprintf(fl2, sizeof(fl2), "R4 short"); break;
            case FAULT_C1_OPEN:   snprintf(fl2, sizeof(fl2), "C1 open"); break;
            case FAULT_C1_DOUBLE: snprintf(fl2, sizeof(fl2), "C1 double"); break;
            case FAULT_C2_OPEN:   snprintf(fl2, sizeof(fl2), "C2 open"); break;
            case FAULT_C2_DOUBLE: snprintf(fl2, sizeof(fl2), "C2 double"); break;
            case FAULT_C3_OPEN:   snprintf(fl2, sizeof(fl2), "C3 open"); break;
            case FAULT_C3_DOUBLE: snprintf(fl2, sizeof(fl2), "C3 double"); break;
            default:              snprintf(fl2, sizeof(fl2), "Unknown fault"); break;
        }
        flines[0] = fl1; flines[1] = fl2;
        Panel_ShowResult(flines, 2);
    }
}

/* ======================== 主函数 ======================== */

int main(void)
{
    MeasureResult baseline;

    SysTick_DelayInit();
    USART_Debug_Init(115200);
    printf("[INIT] SysTick OK\r\n");
    printf("[INIT] USART1 OK, 115200 baud\r\n");
    AD9851_GPIO_Init();
    AD9851_SetFrequency(TEST_FREQ_HZ);
    printf("[INIT] AD9851 OK, freq=%lu Hz\r\n", (unsigned long)TEST_FREQ_HZ);

    ADC_Sample_Init();
    printf("[INIT] ADC1+DMA1+TIM3 OK\r\n");
    Relay_Init();
    printf("[INIT] Relay OK\r\n");
    Key_Init();
    printf("[INIT] KEY EXTI OK (PC13)\r\n");

    printf("==== System Init Done ====\r\n");
    printf("[INIT] CoreClock=%lu Hz, SysTick started\r\n", SystemCoreClock);

    /* 开机测基准 */
    Delay_ms(200);
    Measure_All(&baseline);
    FaultDetect_SetBaseline(&baseline);
    printf("Baseline captured:\r\n");
    PrintMeasureResult(&baseline);

    /* LCD + 触摸 */
    printf("[INIT] Starting ILI9341...\r\n");
    ILI9341_Init();
    printf("[INIT] ILI9341 OK, ID=0x%04X\r\n", (unsigned)ILI9341_ReadID());
    XPT2046_Init();
    printf("[INIT] XPT2046 OK\r\n");
    Panel_Init();
    printf("[INIT] Panel OK, mode=%d\r\n", (int)Panel_GetMode());

    /* 开机显示基准 */
    DisplayResults(&baseline);

    printf("[MAIN] Entering main loop...\r\n");
    uint32_t lastHeartbeat = g_systick_ms;

    while (1)
    {
        /* 每 2 秒输出一次心跳 */
        if (g_systick_ms - lastHeartbeat >= 2000)
        {
            lastHeartbeat = g_systick_ms;
            printf("[LOOP] alive, tick=%lu ms, mode=%d\r\n",
                   (unsigned long)g_systick_ms, (int)Panel_GetMode());
        }
        XPT2046_TouchEvenHandler();
        
        /* 短按 KEY：循环切换 Mode1→Mode2→Mode3→Mode1 */
        if (Key_GetSwitchRequest())
        {
            PanelMode_t m = Panel_GetMode();
            printf("[KEY] switch from mode %d\r\n", (int)m);
            if      (m == MODE_MEASURE) Panel_SetMode(MODE_FAULT);
            else                        Panel_SetMode(MODE_MEASURE);

            printf("Mode switched to: %d\r\n", (int)Panel_GetMode());

            if (Panel_GetMode() == MODE_MEASURE)
                RunMeasureAndDisplay();
            else if (Panel_GetMode() == MODE_FAULT)
                RunFaultDetectAndDisplay();
            else
                RunMeasureAndDisplay();  /* MODE_DEBUG: 测量+显示Debug */
        }

        /* 触摸屏按钮（Mode3无按钮，靠KEY退出） */
        if (Panel_IsNewAction())
        {
            PanelAction_t act = Panel_FetchAction();
            printf("[TOUCH] action=%d\r\n", (int)act);

            if (act == ACTION_MEASURE_ALL)
                RunMeasureAndDisplay();
            else if (act == ACTION_FAULT_DETECT)
                RunFaultDetectAndDisplay();
        }
    }
}