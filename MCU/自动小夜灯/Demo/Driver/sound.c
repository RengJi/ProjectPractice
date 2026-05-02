#include "sound.h" 


/**
  *@brief  初始化麦克风传感器外设配置
  *@param  无
  *@return 无
  */
void sound_init(void)
{
	RCC_APB2PeriphClockCmd(SOUND_CLK, ENABLE);
	
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_EXTILineConfig(SOUND_PORTSOURCE, SOUND_PINSOURCE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Pin = SOUND_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SOUND_PORT, &GPIO_InitStructure);
	
	EXTI_InitTypeDef EXIT_InitStructure;
	EXIT_InitStructure.EXTI_Line = SOUND_EXTI_LINE;
	EXIT_InitStructure.EXTI_LineCmd = ENABLE;
	EXIT_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXIT_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_Init(&EXIT_InitStructure);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = SOUND_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
	
}
