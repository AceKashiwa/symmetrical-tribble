#include "adc.h"

// DMA 缓冲区: 布局 = [rank1_pair, rank2_pair, rank3_pair, rank1_pair, ...]
// 每 32 位 = [ADC2高16位 | ADC1低16位]
__IO uint32_t ADC_ConvertedValue[ADC_DMA_BUF_SIZE] = {0};
__IO uint8_t  ADC_BufferReady = 0;

/**
  * @brief  ADC GPIO 初始化（6通道，全部GPIOC）
  */
static void ADCx_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	ADCx_1_GPIO_APBxClock_FUN(ADCx_1_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;

	// ADC1: PC0/CH10, PC1/CH11, PC2/CH12
	GPIO_InitStructure.GPIO_Pin = ADC1_CH1_PIN | ADC1_CH2_PIN | ADC1_CH3_PIN;
	GPIO_Init(ADCx_1_PORT, &GPIO_InitStructure);

	// ADC2: PC3/CH13, PC4/CH14, PC5/CH15
	GPIO_InitStructure.GPIO_Pin = ADC2_CH1_PIN | ADC2_CH2_PIN | ADC2_CH3_PIN;
	GPIO_Init(ADCx_2_PORT, &GPIO_InitStructure);
}

/**
  * @brief  配置双ADC规则同步模式（RegSimult）+ DMA，最大采样率
  * @note   ADC时钟=12MHz, 采样=1.5周期, 3通道扫描=3.5μs → 285.7 kHz/通道
  */
static void ADCx_Mode_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;
	ADC_InitTypeDef ADC_InitStructure;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	ADCx_1_APBxClock_FUN(ADCx_1_CLK, ENABLE);
	ADCx_2_APBxClock_FUN(ADCx_2_CLK, ENABLE);

	/* ------------------DMA---------------- */
	DMA_DeInit(ADC_DMA_CHANNEL);
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&(ADCx_1->DR));
	DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)ADC_ConvertedValue;
	DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
	DMA_InitStructure.DMA_BufferSize         = ADC_DMA_BUF_SIZE;
	DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Word;
	DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Word;
	DMA_InitStructure.DMA_Mode               = DMA_Mode_Circular;
	DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
	DMA_Init(ADC_DMA_CHANNEL, &DMA_InitStructure);
	DMA_Cmd(ADC_DMA_CHANNEL, ENABLE);

	// TC 中断通知主循环
	DMA_ITConfig(ADC_DMA_CHANNEL, DMA_IT_TC, ENABLE);
	NVIC_InitTypeDef nv;
	nv.NVIC_IRQChannel    = DMA1_Channel1_IRQn;
	nv.NVIC_IRQChannelPreemptionPriority = 1;
	nv.NVIC_IRQChannelSubPriority        = 0;
	nv.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nv);

	// ADC时钟 = PCLK2 / 6 = 12 MHz
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);

	/* ----------------ADC1（主）--------------------- */
	ADC_InitStructure.ADC_Mode               = ADC_Mode_RegSimult;
	ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
	ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel       = NOFCHANEL;
	ADC_Init(ADCx_1, &ADC_InitStructure);

	// 3通道，最短采样时间 → 最大速率
	ADC_RegularChannelConfig(ADCx_1, ADC1_CH1_CHANNEL, 1,
	                         ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig(ADCx_1, ADC1_CH2_CHANNEL, 2,
	                         ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig(ADCx_1, ADC1_CH3_CHANNEL, 3,
	                         ADC_SampleTime_1Cycles5);
	ADC_DMACmd(ADCx_1, ENABLE);

	/* ----------------ADC2（从）--------------------- */
	ADC_InitStructure.ADC_NbrOfChannel = NOFCHANEL;
	ADC_Init(ADCx_2, &ADC_InitStructure);

	ADC_RegularChannelConfig(ADCx_2, ADC2_CH1_CHANNEL, 1,
	                         ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig(ADCx_2, ADC2_CH2_CHANNEL, 2,
	                         ADC_SampleTime_1Cycles5);
	ADC_RegularChannelConfig(ADCx_2, ADC2_CH3_CHANNEL, 3,
	                         ADC_SampleTime_1Cycles5);
	ADC_ExternalTrigConvCmd(ADC2, ENABLE);

	/* ----------------校准--------------------- */
	ADC_Cmd(ADCx_1, ENABLE);
	ADC_ResetCalibration(ADCx_1);
	while (ADC_GetResetCalibrationStatus(ADCx_1));
	ADC_StartCalibration(ADCx_1);
	while (ADC_GetCalibrationStatus(ADCx_1));

	ADC_Cmd(ADCx_2, ENABLE);
	ADC_ResetCalibration(ADCx_2);
	while (ADC_GetResetCalibrationStatus(ADCx_2));
	ADC_StartCalibration(ADCx_2);
	while (ADC_GetCalibrationStatus(ADCx_2));

	ADC_SoftwareStartConvCmd(ADCx_1, ENABLE);
}

/**
  * @brief  ADC 初始化
  */
void ADCx_Init(void)
{
	ADCx_GPIO_Config();
	ADCx_Mode_Config();
}

/**
  * @brief  DMA1_Channel1 中断: 满一圈置标志
  */
void DMA1_Channel1_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TC1))
	{
		DMA_ClearITPendingBit(DMA1_IT_TC1);
		ADC_BufferReady = 1;
	}
}
/*********************************************END OF FILE**********************/
