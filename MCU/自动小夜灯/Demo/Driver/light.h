#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "stm32f10x.h"                  // Device header

#define LIGHT_CLK  (RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1)
#define LIGHT_PIN  GPIO_Pin_1
#define LIGHT_PORT GPIOA

/**
  *@brief  初始化外设
  *@param  无
  *@return 无
  */
void light_ao_gpio_init(void);

/**
  *@brief  获取光强数据
  *@param  无
  *@return ADC获取到的光强
  */
uint16_t adc_get_lightval(void);

#endif
