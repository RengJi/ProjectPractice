#include "body_detect.h"

/**
  *@brief  初始化麦克风传感器外设配置
  *@param  无
  *@return 无
  */
void body_detect_init(void)
{
	RCC_APB2PeriphClockCmd(BODY_DETECT_CLK, ENABLE);
	
	/* 禁止JTAG功能，将PB3和PB4当成普通引脚用 */
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); 
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_EXTILineConfig(BODY_DETECT_PORTSOURCE, BODY_DETECT_PINSOURCE);
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
	GPIO_InitStructure.GPIO_Pin = BODY_DETECT_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BODY_DETECT_PORT, &GPIO_InitStructure);
	
	EXTI_InitTypeDef EXIT_InitStructure;
	EXIT_InitStructure.EXTI_Line = BODY_DETECT_EXTI_LINE;
	EXIT_InitStructure.EXTI_LineCmd = ENABLE;
	EXIT_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXIT_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_Init(&EXIT_InitStructure);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = BODY_DETECT_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
}
