#ifndef __BODY_DETECT_H__
#define __BODY_DETECT_H__


#include "stm32f10x.h"                  // Device header

#define BODY_DETECT_CLK  (RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO)
#define BODY_DETECT_PORT GPIOB
#define BODY_DETECT_PIN  GPIO_Pin_4
#define BODY_DETECT_PORTSOURCE GPIO_PortSourceGPIOB
#define BODY_DETECT_PINSOURCE  GPIO_PinSource4
#define BODY_DETECT_EXTI_LINE  EXTI_Line4
#define BODY_DETECT_IRQn       EXTI4_IRQn

/**
  *@brief  初始化麦克风传感器外设配置
  *@param  无
  *@return 无
  */
void body_detect_init(void);

#endif
