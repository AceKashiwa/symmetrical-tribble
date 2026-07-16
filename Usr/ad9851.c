#include "ad9851.h"
#include "config.h"

/* ---------------------------------------------------------------------
 * AD9851 串行模式驱动
 * 时序：先送 32bit 频率控制字（LSB字节先发，每字节内LSB先发），
 *      再送 1 个控制字节（bit0 = 6x 参考时钟倍频使能），
 *      每个 bit 数据建立后拉高再拉低 W_CLK，40bit 全部发送完毕后
 *      拉高再拉低 FQ_UD 锁存生效。
 * ---------------------------------------------------------------------
 */

static void AD9851_DelayUs(volatile uint32_t us) {
  /* 粗略延时，72MHz主频下的空循环，仅用于满足AD9851建立时间(~ns级)，
     余量足够，不追求精确 */
  volatile uint32_t i;
  for (i = 0; i < us * 8; i++) {
    __NOP();
  }
}

void AD9851_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

  GPIO_InitStructure.GPIO_Pin = AD9851_W_CLK_PIN | AD9851_FQ_UD_PIN |
                                AD9851_RESET_PIN | AD9851_DATA_PIN |
                                AD9851_D0_PIN | AD9851_D1_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_ResetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  GPIO_ResetBits(AD9851_RESET_PORT, AD9851_RESET_PIN);
  GPIO_ResetBits(AD9851_DATA_PORT, AD9851_DATA_PIN);

  GPIO_SetBits(AD9851_D0_PORT, AD9851_D0_PIN);
  GPIO_SetBits(AD9851_D1_PORT, AD9851_D1_PIN);

  AD9851_Reset();
}

void AD9851_Reset(void) {
  /* datasheet 初始化时序：RESET 高电平脉冲 -> W_CLK 脉冲 -> FQ_UD 脉冲，
     之后芯片进入串行加载模式 */
  GPIO_SetBits(AD9851_RESET_PORT, AD9851_RESET_PIN);
  AD9851_DelayUs(2);
  GPIO_ResetBits(AD9851_RESET_PORT, AD9851_RESET_PIN);
  AD9851_DelayUs(2);

  GPIO_SetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
  AD9851_DelayUs(1);
  GPIO_ResetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
  AD9851_DelayUs(1);

  GPIO_SetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  AD9851_DelayUs(1);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  AD9851_DelayUs(1);
}

static void AD9851_WriteByte(uint8_t data) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    if (data & 0x01) {
      GPIO_SetBits(AD9851_DATA_PORT, AD9851_DATA_PIN);
    } else {
      GPIO_ResetBits(AD9851_DATA_PORT, AD9851_DATA_PIN);
    }
    data >>= 1;

    GPIO_SetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
    AD9851_DelayUs(1);
    GPIO_ResetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
    AD9851_DelayUs(1);
  }
}

/* 设置输出频率，freq_hz 支持到约 70MHz（本设计只用到 <=300kHz） */
void AD9851_SetFrequency(uint32_t freq_hz) {
  uint32_t tuning_word;
  uint8_t b0, b1, b2, b3;

  tuning_word =
      (uint32_t)((double)freq_hz * 4294967296.0 / AD9851_REFCLK_HZ + 0.5);

  b0 = (uint8_t)(tuning_word & 0xFF);
  b1 = (uint8_t)((tuning_word >> 8) & 0xFF);
  b2 = (uint8_t)((tuning_word >> 16) & 0xFF);
  b3 = (uint8_t)((tuning_word >> 24) & 0xFF);

  AD9851_WriteByte(b0);
  AD9851_WriteByte(b1);
  AD9851_WriteByte(b2);
  AD9851_WriteByte(b3);
  AD9851_WriteByte(0x01); /* 控制字节：bit0=1 使能 6x 参考时钟倍频，相位位=0 */

  GPIO_SetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  AD9851_DelayUs(1);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
}
