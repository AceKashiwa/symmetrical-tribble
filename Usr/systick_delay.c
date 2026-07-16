#include "systick_delay.h"

volatile uint32_t g_systick_ms = 0;

void SysTick_DelayInit(void)
{
    /* 72MHz 主频，1ms 一次 SysTick 中断 */
    if (SysTick_Config(SystemCoreClock / 1000))
    {
        while (1)
            ; /* 配置失败 */
    }
}

void Delay_ms(uint32_t ms)
{
    uint32_t start = g_systick_ms;
    while ((g_systick_ms - start) < ms)
        ;
}

/* 注意：如果工程自带的 stm32f10x_it.c 里已有 SysTick_Handler 的空实现，
 * 请删除那边的定义，避免重复定义链接错误。 */
void SysTick_Handler(void)
{
    g_systick_ms++;
}
