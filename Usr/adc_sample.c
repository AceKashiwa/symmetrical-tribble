#include "adc_sample.h"
#include "config.h"
#include <stdio.h>

/* DMA 缓冲区：按 [Uin0,Iin0,Uout0, Uin1,Iin1,Uout1, ...] 交织存放 */
static uint16_t s_adc_buf[ADC_SAMPLE_POINTS * ADC_CH_NUM];

/* 定时器输入时钟，按 72MHz 主频、APB1 定时器时钟 72MHz 的常见配置假设，
 * 如实际时钟树不同，请修改这里 */
#define TIM3_CLK_HZ   72000000UL

static void ADC_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}

static void ADC1_Config(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    /* ADC 时钟不要超过 14MHz，72MHz/6=12MHz */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = ADC_CH_NUM;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_CH_UIN, 1, ADC_SampleTime_28Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_CH_IIN, 2, ADC_SampleTime_28Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_CH_UOUT, 3, ADC_SampleTime_28Cycles5);

    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
        ;
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
        ;
}

static void DMA1_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)s_adc_buf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = ADC_SAMPLE_POINTS * ADC_CH_NUM;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal; /* 单次采集一窗口后停止 */
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
}

static void TIM3_Config(uint32_t sample_rate_hz)
{
    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    uint32_t arr;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    arr = (TIM3_CLK_HZ / sample_rate_hz);
    if (arr > 0) arr -= 1;

    TIM_InitStructure.TIM_Period = arr;
    TIM_InitStructure.TIM_Prescaler = 0;
    TIM_InitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_InitStructure);

    TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);
    TIM_Cmd(TIM3, DISABLE);
}

void ADC_Sample_Init(void)
{
    ADC_GPIO_Config();
    ADC1_Config();
    DMA1_Config();
    TIM3_Config(ADC_SAMPLE_RATE_HZ_DEFAULT);
}

void ADC_Sample_Once(uint32_t sample_rate_hz)
{
    /* 重新配置采样率 */
    TIM_Cmd(TIM3, DISABLE);
    TIM3_Config(sample_rate_hz);

    /* 重新装载 DMA（Normal 模式每次用完计数清零，需要重新 Init） */
    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA1_Config();
    DMA_ClearFlag(DMA1_FLAG_TC1);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    TIM_SetCounter(TIM3, 0);
    TIM_Cmd(TIM3, ENABLE);

    /* 阻塞等待整窗数据采集完成 */
    while (DMA_GetFlagStatus(DMA1_FLAG_TC1) == RESET)
        ;

    TIM_Cmd(TIM3, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
}

void ADC_Sample_GetChannel(uint8_t ch_idx, uint16_t **buf, uint32_t *count)
{
    static uint16_t tmp[ADC_SAMPLE_POINTS];
    uint32_t i;

    for (i = 0; i < ADC_SAMPLE_POINTS; i++)
    {
        tmp[i] = s_adc_buf[i * ADC_CH_NUM + ch_idx];
    }
    *buf = tmp;
    *count = ADC_SAMPLE_POINTS; 
}

float ADC_Sample_GetVpp(uint8_t ch_idx)
{
    uint32_t i;
    uint16_t vmax = 0, vmin = 0xFFFF, v;

    for (i = 0; i < ADC_SAMPLE_POINTS; i++)
    {
        v = s_adc_buf[i * ADC_CH_NUM + ch_idx];
        if (v > vmax) vmax = v;
        if (v < vmin) vmin = v;
    }
    return (float)(vmax - vmin) * ADC_VREF / ADC_FULL_SCALE;
}

float ADC_Sample_GetVavg(uint8_t ch_idx)
{
    uint32_t i;
    uint32_t sum = 0;

    for (i = 0; i < ADC_SAMPLE_POINTS; i++)
    {
        sum += s_adc_buf[i * ADC_CH_NUM + ch_idx];
    }
    return ((float)sum / ADC_SAMPLE_POINTS) * ADC_VREF / ADC_FULL_SCALE;
}

/* ---- ADC 原始数据 + 计算值 串口输出 ---- */
static const char *s_ch_names[ADC_CH_NUM] = { "Uin", "Iin", "Uout" };

void ADC_Sample_DumpSerial(uint8_t print_raw_samples, uint16_t raw_count)
{
    uint8_t ch;
    uint16_t i;

    printf("==== ADC Dump (%d samples/ch) ====\r\n", (int)ADC_SAMPLE_POINTS);

    /* ---- 摘要：每通道 Vpp / Vavg / Vmin / Vmax ---- */
    printf("Ch   Vpp(V)   Vavg(V)   Vmin(raw) Vmax(raw)\r\n");
    printf("-------------------------------------------\r\n");
    for (ch = 0; ch < ADC_CH_NUM; ch++)
    {
        uint16_t vmin = 0xFFFF, vmax = 0;
        uint32_t sum = 0;

        for (i = 0; i < ADC_SAMPLE_POINTS; i++)
        {
            uint16_t v = s_adc_buf[i * ADC_CH_NUM + ch];
            if (v < vmin) vmin = v;
            if (v > vmax) vmax = v;
            sum += v;
        }

        float vpp  = (float)(vmax - vmin) * ADC_VREF / ADC_FULL_SCALE;
        float vavg = (float)sum / ADC_SAMPLE_POINTS * ADC_VREF / ADC_FULL_SCALE;

        printf("%-4s  %-8.4f %-8.4f %-10u %u\r\n",
               s_ch_names[ch], vpp, vavg, vmin, vmax);
    }

    /* ---- 原始采样点（可选） ---- */
    if (print_raw_samples)
    {
        if (raw_count > ADC_SAMPLE_POINTS)
            raw_count = ADC_SAMPLE_POINTS;
        if (raw_count == 0)
            raw_count = 20; /* 默认打前20个点 */

        printf("\r\n--- Raw samples (first %u of %u) ---\r\n", raw_count, (unsigned)ADC_SAMPLE_POINTS);
        printf("Idx   Uin     Iin     Uout\r\n");
        printf("----------------------------\r\n");

        for (i = 0; i < raw_count; i++)
        {
            /* 每10行打一次表头 */
            if (i > 0 && (i % 20) == 0)
                printf("Idx   Uin     Iin     Uout\r\n");

            printf("%-5u %-7u %-7u %u\r\n",
                   (unsigned)i,
                   s_adc_buf[i * ADC_CH_NUM + 0],
                   s_adc_buf[i * ADC_CH_NUM + 1],
                   s_adc_buf[i * ADC_CH_NUM + 2]);
        }
    }

    printf("==== ADC Dump End ====\r\n\r\n");
}

void ADC_Sample_GetRawMinMax(uint8_t ch_idx, uint16_t *vmin, uint16_t *vmax)
{
    uint32_t i;
    uint16_t mn = 0xFFFF, mx = 0;

    for (i = 0; i < ADC_SAMPLE_POINTS; i++)
    {
        uint16_t v = s_adc_buf[i * ADC_CH_NUM + ch_idx];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    *vmin = mn;
    *vmax = mx;
}
