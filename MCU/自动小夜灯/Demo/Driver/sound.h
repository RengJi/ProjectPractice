#ifndef __SOUND_H__
#define __SOUND_H__


#include "stm32f10x.h"                  // Device header

#define SOUND_CLK  (RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO)
#define SOUND_PORT GPIOB
#define SOUND_PIN  GPIO_Pin_1
#define SOUND_PORTSOURCE GPIO_PortSourceGPIOB
#define SOUND_PINSOURCE  GPIO_PinSource1
#define SOUND_EXTI_LINE  EXTI_Line1
#define SOUND_IRQn       EXTI1_IRQn

/**
  *@brief  初始化麦克风传感器外设配置
  *@param  无
  *@return 无
  */
void sound_init(void);

#endif
