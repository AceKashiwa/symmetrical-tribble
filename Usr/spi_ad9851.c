#include "spi_ad9851.h"
#include "stm32f10x_gpio.h"

static void AD9851_DelayShort(volatile u32 cnt) {
  while (cnt--) {
  }
}

static void AD9851_SendByte(u8 data) {
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
  }
  SPI_I2S_SendData(SPI1, data);
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET) {
  }
}

void SPI_AD9851_Init(void) {
  SPI_InitTypeDef SPI_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

  GPIO_InitStructure.GPIO_Pin = AD9851_FQ_UD_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(AD9851_FQ_UD_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = AD9851_W_CLK_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(AD9851_W_CLK_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = AD9851_RESET_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_Init(AD9851_RESET_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = AD9851_D7_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(AD9851_D7_PORT, &GPIO_InitStructure);

  SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_LSB;
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(SPI1, &SPI_InitStructure);
}

void AD9851_Reset(void) {
  GPIO_ResetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  GPIO_ResetBits(AD9851_RESET_PORT, AD9851_RESET_PIN);
  AD9851_DelayShort(10);

  GPIO_SetBits(AD9851_RESET_PORT, AD9851_RESET_PIN);
  AD9851_DelayShort(10);
  GPIO_ResetBits(AD9851_RESET_PORT, AD9851_RESET_PIN);
  AD9851_DelayShort(10);

  GPIO_SetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);
  AD9851_DelayShort(2);
  GPIO_ResetBits(AD9851_W_CLK_PORT, AD9851_W_CLK_PIN);

  GPIO_SetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  AD9851_DelayShort(2);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
}

void AD9851_SetFrequency(u32 freq_hz) { AD9851_SetFrequencyPhase(freq_hz, 0); }

void AD9851_SetFrequencyPhase(u32 freq_hz, u8 phase_5bit) {
  u32 w;
  u8 ctrl_byte;
  uint64_t temp;

  temp = ((uint64_t)freq_hz << 32) / AD9851_REF_CLK_HZ;
  w = (u32)temp;

  ctrl_byte = (u8)((phase_5bit & 0x1F) << 3);

#if AD9851_USE_REFCLK_MULTIPLIER
  ctrl_byte |= 0x01;
#endif

  AD9851_SendByte((u8)(w & 0xFF));
  AD9851_SendByte((u8)((w >> 8) & 0xFF));
  AD9851_SendByte((u8)((w >> 16) & 0xFF));
  AD9851_SendByte((u8)((w >> 24) & 0xFF));
  AD9851_SendByte(ctrl_byte);

  GPIO_SetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  AD9851_DelayShort(2);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
}

/* 关断输出(掉电模式) */
void AD9851_PowerDown(void) {
  u8 ctrl_byte = 0x04; /* bit2 = power down */

  AD9851_SendByte(0x00);
  AD9851_SendByte(0x00);
  AD9851_SendByte(0x00);
  AD9851_SendByte(0x00);
  AD9851_SendByte(ctrl_byte);

  GPIO_SetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
  AD9851_DelayShort(2);
  GPIO_ResetBits(AD9851_FQ_UD_PORT, AD9851_FQ_UD_PIN);
}

void AD9851_PowerUp(void) { AD9851_SetFrequency(0); }