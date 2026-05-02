#ifndef __RELAY_H__
#define __RELAY_H__

#include "stm32f10x.h"                  // Device header

#define RELAY_CLK  RCC_APB2Periph_GPIOB
#define RELAY_PORT GPIOB
#define RELAY_PIN  GPIO_Pin_8

/**
  *@brief  继电器引脚初始化
  *@param  无
  *@return 无
  */
void relay_pin_init(void);

/**
  *@brief  关闭继电器
  *@param  无
  *@return 无
  */
void relay_off(void);

/**
  *@brief  打开继电器
  *@param  无
  *@return 无
  */
void relay_on(void);

/**
  *@brief  切换继电器状态
  *@param  无
  *@return 无
  */
void relay_toggle(void);

#endif
