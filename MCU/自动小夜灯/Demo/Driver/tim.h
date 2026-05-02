#ifndef __TIM_H__
#define __TIM_H__

#include "stm32f10x.h"                  // Device header

#define IR_TIM_CLK RCC_APB1Periph_TIM2
#define IR_TIM TIM2
#define IR_IT_IRQn TIM2_IRQn

/**
 * @brief  红外接收需要的中断定时器初始化
 * @param  无
 * @return 无
 */
void ir_tim_init(void);



#endif

