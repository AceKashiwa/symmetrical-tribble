#include "key.h"
#include "config.h"

static volatile uint8_t s_switch_req = 0;

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = KEY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; /* 内部上拉，按键按下拉低 */
    GPIO_Init(KEY_PORT, &GPIO_InitStructure);

    GPIO_EXTILineConfig(KEY_EXTI_PORTSRC, KEY_EXTI_PINSRC);

    EXTI_InitStructure.EXTI_Line = KEY_EXTI_LINE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = KEY_EXTI_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

uint8_t Key_GetSwitchRequest(void)
{
    uint8_t r = s_switch_req;
    s_switch_req = 0;
    return r;
}

/* 注意：如果工程自带的 stm32f10x_it.c 里已有 EXTI15_10_IRQHandler 的空实现，
 * 请删除那边的定义（或把下面的内容合并过去），避免重复定义链接错误。 */
void EXTI15_10_IRQHandler(void)
{
    static volatile uint32_t last_tick = 0;
    extern volatile uint32_t g_systick_ms; /* 由 SysTick 中断累加，见 main.c */

    if (EXTI_GetITStatus(KEY_EXTI_LINE) != RESET)
    {
        /* 简单软件防抖：20ms 内的连续触发忽略 */
        if (g_systick_ms - last_tick > 20)
        {
            s_switch_req = 1;
            last_tick = g_systick_ms;
        }
        EXTI_ClearITPendingBit(KEY_EXTI_LINE);
    }
}
