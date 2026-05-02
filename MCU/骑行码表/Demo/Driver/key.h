#ifndef __KEY_H__
#define __KEY_H__

#include "stm32f10x.h"                  // Device header

#define KEY_CLK (RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC)
#define KEY1_PIN GPIO_Pin_14
#define KEY1_PORT GPIOC
#define KEY2_PIN GPIO_Pin_0
#define KEY2_PORT GPIOB

/** 
 * @brief  初始化按键
 * @param  无
 * @return 无
 */
void key_init(void);

#endif
