#ifndef __BUZZER_H__
#define __BUZZER_H__

#include "stm32f10x.h"                  // Device header
#include "delay.h"

#define BUZZER_CLK RCC_APB2Periph_GPIOB
#define BUZZER_PORT GPIOB
#define BUZZER_PIN GPIO_Pin_12

/**
 * @brief  蜂鸣器引脚初始化
 * @param  无
 * @return 无
 */
void buzzer_init(void);

/**
 * @brief  使蜂鸣器响指定秒数
 * @param  sec 需要响的秒数
 * @return 无
 */
void buzzer(uint8_t sec);

#endif
