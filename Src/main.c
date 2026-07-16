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

/* 在LCD右侧显示基准参数 */
static void ShowBaselineOnLCD(const MeasureResult *r)
{
    static char l0[32], l1[32], l2[32];
    const char *lines[3];

    snprintf(l0, sizeof(l0), "Rin=%.0f  Rout=%.1f", r->rin_ohm, r->rout_ohm);
    snprintf(l1, sizeof(l1), "Gain=%.1fdB  Fc=%.0fHz", r->gain_db, r->cutoff_freq_hz);
    snprintf(l2, sizeof(l2), "Baseline OK");

    lines[0] = l0;
    lines[1] = l1;
    lines[2] = l2;
    Panel_ShowResult(lines, 3);
}

/* ======================== Mode1: 测量处理 ======================== */
static void Mode1_HandleAction(PanelAction_t action)
{
    static char lbuf[8][40];
    const char *lines[8];
    int lc = 0;

    switch (action)
    {
        case ACTION_MEASURE_ALL:
        {
            MeasureResult r;
            Panel_Debug_Clear();
            Panel_Debug_ShowFloat("Measuring...", 0.0f, "");

            Measure_All(&r);
            PrintMeasureResult(&r);

            /* 输出全部 ADC 原始采样数据到串口 */
            ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);

            /* 显示结果 */
            snprintf(lbuf[0], sizeof(lbuf[0]), "Rin =%.1f ohm", r.rin_ohm);
            snprintf(lbuf[1], sizeof(lbuf[1]), "Rout=%.1f ohm", r.rout_ohm);
            snprintf(lbuf[2], sizeof(lbuf[2]), "Gain(1k)=%.2fdB", r.gain_db);
            snprintf(lbuf[3], sizeof(lbuf[3]), "Cutoff=%.0f Hz", r.cutoff_freq_hz);
            for (lc = 0; lc < 4; lc++) lines[lc] = lbuf[lc];
            Panel_ShowResult(lines, lc);

            /* 绘制幅频特性曲线 */
            Panel_DrawFreqResp(r.freqresp, FREQRESP_POINT_NUM,
                               r.gain_db, r.cutoff_freq_hz);

            /* Debug 区显示中间信号 + ADC 原始值 */
            {
                uint16_t vmin, vmax;
                Panel_Debug_Clear();
                ADC_Sample_GetRawMinMax(ADC_IDX_UIN, &vmin, &vmax);
                Panel_Debug_ShowInt("Uin raw", vmax - vmin, "(pp)");
                ADC_Sample_GetRawMinMax(ADC_IDX_IIN, &vmin, &vmax);
                Panel_Debug_ShowInt("Iin raw", vmax - vmin, "(pp)");
                ADC_Sample_GetRawMinMax(ADC_IDX_UOUT, &vmin, &vmax);
                Panel_Debug_ShowInt("Uout raw", vmax - vmin, "(pp)");
            }
            break;
        }

        case ACTION_INPUT_R:
        {
            float rin = Measure_InputResistance();
            printf("Input R = %.2f ohm\r\n", rin);
            ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);

            snprintf(lbuf[0], sizeof(lbuf[0]), "Input Resistance");
            snprintf(lbuf[1], sizeof(lbuf[1]), "Rin = %.1f ohm", rin);
            if (rin > 1.0e6f)
                snprintf(lbuf[2], sizeof(lbuf[2]), "(open circuit)");
            else if (rin < 10.0f)
                snprintf(lbuf[2], sizeof(lbuf[2]), "(near short)");
            else
                lbuf[2][0] = '\0';
            for (lc = 0; lc < 3; lc++) lines[lc] = lbuf[lc];
            Panel_ShowResult(lines, 3);
            {
                uint16_t vmin, vmax;
                Panel_Debug_Clear();
                ADC_Sample_GetRawMinMax(ADC_IDX_UIN, &vmin, &vmax);
                Panel_Debug_ShowInt("Uin pp", vmax - vmin, "raw");
                ADC_Sample_GetRawMinMax(ADC_IDX_IIN, &vmin, &vmax);
                Panel_Debug_ShowInt("Iin pp", vmax - vmin, "raw");
                Panel_Debug_ShowFloat("Rin", rin, "ohm");
            }
            break;
        }

        case ACTION_OUTPUT_R:
        {
            float rout = Measure_OutputResistance();
            printf("Output R = %.2f ohm\r\n", rout);
            ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);

            snprintf(lbuf[0], sizeof(lbuf[0]), "Output Resistance");
            snprintf(lbuf[1], sizeof(lbuf[1]), "Rout = %.1f ohm", rout);
            for (lc = 0; lc < 2; lc++) lines[lc] = lbuf[lc];
            Panel_ShowResult(lines, 2);
            {
                uint16_t vmin, vmax;
                Panel_Debug_Clear();
                ADC_Sample_GetRawMinMax(ADC_IDX_UOUT, &vmin, &vmax);
                Panel_Debug_ShowInt("Uout pp", vmax - vmin, "raw");
                Panel_Debug_ShowFloat("Rout", rout, "ohm");
            }
            break;
        }

        case ACTION_GAIN_1K:
        {
            float gain = Measure_Gain_dB();
            printf("Gain(1kHz) = %.2f dB\r\n", gain);
            ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);

            snprintf(lbuf[0], sizeof(lbuf[0]), "Voltage Gain @1kHz");
            snprintf(lbuf[1], sizeof(lbuf[1]), "Gain = %.2f dB", gain);
            for (lc = 0; lc < 2; lc++) lines[lc] = lbuf[lc];
            Panel_ShowResult(lines, 2);
            {
                uint16_t vmin, vmax;
                Panel_Debug_Clear();
                ADC_Sample_GetRawMinMax(ADC_IDX_UIN, &vmin, &vmax);
                Panel_Debug_ShowInt("Uin pp", vmax - vmin, "raw");
                ADC_Sample_GetRawMinMax(ADC_IDX_UOUT, &vmin, &vmax);
                Panel_Debug_ShowInt("Uout pp", vmax - vmin, "raw");
                Panel_Debug_ShowFloat("Gain", gain, "dB");
            }
            break;
        }

        case ACTION_FREQ_RESP:
        {
            FreqRespPoint resp[FREQRESP_POINT_NUM];
            float cutoff = Measure_FreqResponse(resp);

            printf("Freq Response measured, cutoff=%.1f Hz\r\n", cutoff);
            ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);

            /* 找中频增益（1kHz） */
            float mid_gain = 0.0f;
            for (int i = 0; i < FREQRESP_POINT_NUM; i++)
            {
                if (resp[i].freq_hz == 1000.0f) { mid_gain = resp[i].gain_db; break; }
            }

            snprintf(lbuf[0], sizeof(lbuf[0]), "Freq Response");
            snprintf(lbuf[1], sizeof(lbuf[1]), "Cutoff=%.0f Hz", cutoff);
            snprintf(lbuf[2], sizeof(lbuf[2]), "MidGain=%.1f dB", mid_gain);
            for (lc = 0; lc < 3; lc++) lines[lc] = lbuf[lc];
            Panel_ShowResult(lines, 3);

            /* 绘制幅频特性曲线 */
            Panel_DrawFreqResp(resp, FREQRESP_POINT_NUM, mid_gain, cutoff);

            /* Debug */
            {
                uint16_t vmin, vmax;
                Panel_Debug_Clear();
                ADC_Sample_GetRawMinMax(ADC_IDX_UOUT, &vmin, &vmax);
                Panel_Debug_ShowInt("Uout pp", vmax - vmin, "raw");
                Panel_Debug_ShowFloat("Cutoff", cutoff, "Hz");
                Panel_Debug_ShowFloat("MidGain", mid_gain, "dB");
            }
            break;
        }

        default:
            break;
    }
}

/* ======================== Mode2: 故障检测处理 ======================== */
static void Mode2_HandleAction(PanelAction_t action)
{
    static char lbuf[4][40];
    const char *lines[4];

    if (action != ACTION_FAULT_DETECT) return;

    Panel_Debug_Clear();
    Panel_Debug_ShowFloat("Detecting...", 0.0f, "");

    MeasureResult cur;
    Measure_All(&cur);
    PrintMeasureResult(&cur);
    ADC_Sample_DumpSerial(1, ADC_SAMPLE_POINTS);  /* 故障检测时也输出全部 ADC 原始数据 */

    FaultType ft = FaultDetect_Run(&cur);
    const char *ftName = FaultDetect_TypeName(ft);

    printf("---- Fault Detect ---- Result: %s\r\n", ftName);

    snprintf(lbuf[0], sizeof(lbuf[0]), "Fault Detection");
    snprintf(lbuf[1], sizeof(lbuf[1]), "Result: %s", ftName);

    /* 补充描述 */
    switch (ft)
    {
        case FAULT_NONE:
            snprintf(lbuf[2], sizeof(lbuf[2]), "Circuit: NORMAL");
            break;
        case FAULT_R1_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R1 open, Vout~VCC");
            break;
        case FAULT_R1_SHORT:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R1 short, Vout~11V");
            break;
        case FAULT_R2_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R2 open, Vout~4V");
            break;
        case FAULT_R2_SHORT:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R2 short, Vout~VCC");
            break;
        case FAULT_R3_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R3 open, Vout~0.2V");
            break;
        case FAULT_R3_SHORT:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R3 short, Vout~VCC");
            break;
        case FAULT_R4_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R4 open, Rin large");
            break;
        case FAULT_R4_SHORT:
            snprintf(lbuf[2], sizeof(lbuf[2]), "R4 short, Vout~0");
            break;
        case FAULT_C1_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "C1 open, no signal");
            break;
        case FAULT_C1_DOUBLE:
            snprintf(lbuf[2], sizeof(lbuf[2]), "C1 double, phase shift");
            break;
        case FAULT_C2_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "C2 open, gain drop");
            break;
        case FAULT_C2_DOUBLE:
            snprintf(lbuf[2], sizeof(lbuf[2]), "C2 double, lowF gain up");
            break;
        case FAULT_C3_OPEN:
            snprintf(lbuf[2], sizeof(lbuf[2]), "C3 open, cutoff up");
            break;
        case FAULT_C3_DOUBLE:
            snprintf(lbuf[2], sizeof(lbuf[2]), "C3 double, cutoff down");
            break;
        default:
            snprintf(lbuf[2], sizeof(lbuf[2]), "Unknown fault");
            break;
    }

    for (int i = 0; i < 3; i++) lines[i] = lbuf[i];
    Panel_ShowResult(lines, 3);

    /* Debug 面板显示详细中间信号 + ADC 原始值 */
    {
        uint16_t vmin, vmax;
        Panel_Debug_Clear();
        ADC_Sample_GetRawMinMax(ADC_IDX_UIN, &vmin, &vmax);
        Panel_Debug_ShowInt("Uin pp", vmax - vmin, "raw");
        ADC_Sample_GetRawMinMax(ADC_IDX_IIN, &vmin, &vmax);
        Panel_Debug_ShowInt("Iin pp", vmax - vmin, "raw");
        ADC_Sample_GetRawMinMax(ADC_IDX_UOUT, &vmin, &vmax);
        Panel_Debug_ShowInt("Uout pp", vmax - vmin, "raw");
    }
}

/* ======================== 主函数 ======================== */

int main(void)
{
    MeasureResult baseline;

    SysTick_DelayInit();
    USART_Debug_Init(115200);
    AD9851_GPIO_Init();

    AD9851_SetFrequency(TEST_FREQ_HZ);  /* 写入有效40bit数据 */

    ADC_Sample_Init();
    Relay_Init();
    Key_Init();

    printf("==== System Init Done ====\r\n");

    /* 开机测一次基准数据 */
    Delay_ms(200);
    Measure_All(&baseline);
    FaultDetect_SetBaseline(&baseline);
    printf("Baseline captured:\r\n");
    PrintMeasureResult(&baseline);

    /* LCD + 触摸初始化（Panel_Init 内部会切换到横向模式 GramScan(3)） */
    ILI9341_Init();
    XPT2046_Init();
    Panel_Init();
    ShowBaselineOnLCD(&baseline);

    while (1)
    {
        /* 触摸处理（建议5~10ms调用一次） */
        XPT2046_TouchEvenHandler();

        /* 物理按键切换模式 */
        if (Key_GetSwitchRequest())
        {
            if (Panel_GetMode() == MODE_MEASURE)
                Panel_SetMode(MODE_FAULT);
            else
                Panel_SetMode(MODE_MEASURE);

            printf("Mode switched to: %s\r\n",
                   Panel_GetMode() == MODE_MEASURE ? "MEASURE" : "FAULT");
        }

        /* 触摸按钮触发的动作 */
        if (Panel_IsNewAction())
        {
            PanelAction_t act = Panel_FetchAction();

            if (Panel_GetMode() == MODE_MEASURE)
            {
                Mode1_HandleAction(act);
            }
            else
            {
                Mode2_HandleAction(act);
            }
        }
    }
}