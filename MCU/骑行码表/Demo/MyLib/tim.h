#ifndef __TIM_H__
#define __TIM_H__

#include "stm32f10x.h"                  // Device header


/* 计时定时器 */
#define TIMER TIM2
#define TIMER_CLK RCC_APB1Periph_TIM2
#define TIMER_IRQn TIM2_IRQn

/**
 * @brief  计数定时器初始化
 * @param  无
 * @return 无 
 */
void timer_init(void);

/**
 * @brief  获取开始后经过的时间
 * @param  无
 * @return 经过的时间
 */
uint32_t get_sys_ms(void);

#endif 
