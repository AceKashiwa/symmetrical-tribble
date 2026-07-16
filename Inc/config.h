#ifndef __CONFIG_H
#define __CONFIG_H
/* =========================================================================
 * 全局配置文件 —— 所有需要根据实际电路标定 / 修改的参数集中放在这里
 * 芯片：STM32F103VET6（大容量Density产品线，LQFP100，标准外设库 StdPeriph）
 * 说明：本文件里用到的外设(ADC1/DMA1/TIM3/GPIOA/GPIOB/USART1/EXTI)在
 * STM32F103 中低容量/大容量产品线上完全一致，无需因为换成VET6而修改
 * 外设配置代码；VET6只是多出更多GPIO(C/D/E)、ADC2/ADC3、更多定时器和
 * FSMC等资源可供以后扩展使用。
 * =========================================================================
 */

/* ---------------- ADC 三路采样：输入电压 / 输入电流 / 输出电压 ---------------- */
/* 默认占用 PA0/PA1/PA2，如实际电路引脚不同，只需改这里 */
#define ADC_CH_UIN          ADC_Channel_11   /* PC1：输入电压采样 */
#define ADC_CH_IIN          ADC_Channel_12   /* PC2：AD620 电流采样输出 */
#define ADC_CH_UOUT         ADC_Channel_13   /* PC3：输出电压采样 */

#define ADC_VREF            3.3f            /* ADC 参考电压 */
#define ADC_FULL_SCALE      4096.0f         /* 12bit */

/* 采样窗口：每次测量抓取的“每通道”采样点数（3 通道共 3*N 个 DMA 字） */
#define ADC_SAMPLE_POINTS   512

/* 默认采样率（Hz），由 TIM3 触发 ADC 转换；测低频(20Hz)相位时会临时降低采样率 */
#define ADC_SAMPLE_RATE_HZ_DEFAULT   200000UL
#define ADC_SAMPLE_RATE_HZ_LOWFREQ   10000UL   /* 用于 20Hz 相位检测，需覆盖多个周期 */

/* ---------------- 采样电路增益标定（务必用示波器/万用表实测后修改） ---------------- */
#define GAIN_UIN_SAMPLE     50.0f    /* 图3.3 输入电压采样电路放大倍数（约50倍，需实测） */
#define GAIN_UOUT_SAMPLE    1.0f     /* 图3.4 输出电压采样电路的等效比例（需实测标定） */

#define AD620_RG_OHM        470.0f
#define AD620_GAIN          (1.0f + 49400.0f / AD620_RG_OHM)  /* ≈106.1 */
#define R_SENSE_OHM         1000.0f  /* 电流采样电阻（1kΩ，串在输入回路里） */

#define R_LOAD_OHM          2000.0f  /* 用于两次测量法的已知负载电阻，图3.4中继电器控制的2k负载 */

/* ---------------- AD9851 DDS 控制引脚 ---------------- */
#define AD9851_W_CLK_PORT   GPIOB
#define AD9851_W_CLK_PIN    GPIO_Pin_12
#define AD9851_FQ_UD_PORT   GPIOB
#define AD9851_FQ_UD_PIN    GPIO_Pin_13
#define AD9851_RESET_PORT   GPIOB
#define AD9851_RESET_PIN    GPIO_Pin_14
#define AD9851_DATA_PORT    GPIOB
#define AD9851_DATA_PIN     GPIO_Pin_15

#define AD9851_D0_PORT      GPIOB
#define AD9851_D0_PIN       GPIO_Pin_8
#define AD9851_D1_PORT      GPIOB
#define AD9851_D1_PIN       GPIO_Pin_9

#define AD9851_REFCLK_HZ    180000000.0  /* 30MHz 晶振 x6 倍频 */

/* ---------------- 继电器（输出端2k负载通断，两次测量法测输出电阻） ---------------- */
#define RELAY_PORT          GPIOC
#define RELAY_PIN           GPIO_Pin_7

/* ---------------- 模式切换按键（EXTI） ---------------- */
#define KEY_PORT             GPIOC
#define KEY_PIN              GPIO_Pin_13
#define KEY_EXTI_LINE        EXTI_Line13
#define KEY_EXTI_PORTSRC     GPIO_PortSourceGPIOC
#define KEY_EXTI_PINSRC      GPIO_PinSource13
#define KEY_EXTI_IRQ         EXTI15_10_IRQn

/* ---------------- 测量相关常量 ---------------- */
#define TEST_FREQ_HZ         1000UL   /* 报告中"1kHz"测量点：输入电阻/输出电阻/增益 */
#define LOWFREQ_TEST_HZ      20UL     /* 故障5(C1)判断用的低频点 */

/* 幅频特性扫频点（十倍频，报告4.2.1），可根据实际放大电路截止频率范围调整 */
#define FREQRESP_POINT_NUM   6
static const uint32_t FREQRESP_POINTS_HZ[FREQRESP_POINT_NUM] =
{
    20, 200, 1000, 10000, 100000, 300000
};

/* ---------------- 故障判断阈值（以下数值均来自报告的理论分析，
 * 实际需要用你的电路把每种故障人为制造出来后实测标定，仅供起始参考） ---------------- */
#define VCC_V                       12.0f

#define FAULT_VOUT_VCC_TOL          1.0f   /* 判断 Vout≈VCC 的容差 */
#define FAULT_VOUT_R1SHORT_V        11.0f
#define FAULT_VOUT_R1SHORT_TOL      1.0f
#define FAULT_VOUT_R2OPEN_V         4.0f
#define FAULT_VOUT_R2OPEN_TOL       1.0f
#define FAULT_VOUT_R3OPEN_V         0.2f
#define FAULT_VOUT_R3OPEN_TOL       0.15f
#define FAULT_VOUT_R4SHORT_MAX_V    0.1f   /* "几十毫伏" */

#define FAULT_RIN_OPEN_MIN_OHM      1.0e6f /* 视为"很大"(开路级)的阈值 */
#define FAULT_RIN_SHORT_MAX_OHM     10.0f  /* 视为"接近0"(短路级)的阈值 */
#define FAULT_RIN_R4OPEN_MAX_OHM    5.0e5f /* R4开路时Rin略小于R1开路，需用实测数据标定该分界 */

#define FAULT_GAIN_DROP_DB          10.0f  /* 增益相对基准下降超过此值 -> 视为显著降低(C2开路) */
#define FAULT_GAIN_RISE_DB          2.0f   /* 低频增益相对基准上升超过此值 -> C2加倍 */
#define FAULT_HIFREQ_GAIN_RISE_DB   2.0f   /* 高频增益上升 -> C3开路 */
#define FAULT_HIFREQ_GAIN_DROP_DB   2.0f   /* 高频增益下降(截止频率下降) -> C3加倍 */

#define FAULT_PHASE_DIFF_TOL_RAD    0.15f  /* 相位差相对基准变化超过此值 -> C1加倍 */
#define FAULT_GAIN_NEAR_ZERO_DB     (-40.0f) /* 增益接近0(无信号传递) -> C1开路的判据之一 */

#endif
