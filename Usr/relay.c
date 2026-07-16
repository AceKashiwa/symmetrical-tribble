#include "relay.h"
#include "config.h"

void Relay_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = RELAY_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(RELAY_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
}

void Relay_SetLoad(uint8_t on)
{
    if (on)
    {
        GPIO_SetBits(RELAY_PORT, RELAY_PIN);
    }
    else
    {
        GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
    }
}
