#ifndef __TIM_H__
#define __TIM_H__

#include "stm32f10x.h"                  // Device header

/* pwm电机驱动定时器 */
#define PWM_TIM TIM2
#define PWM_CLK RCC_APB1Periph_TIM2 
#define PWM_PIN1 GPIO_Pin_3
#define PWM_PIN2 GPIO_Pin_10
#define PWM_PORT GPIOB

/* 红外接收定时器 */
#define IR_TIM TIM1
#define IR_TIM_CLK (RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1)
#define IR_PIN GPIO_Pin_8
#define IR_PORT GPIOA
#define IR_IRQn TIM1_CC_IRQn

/* 计时定时器 */
#define TIMER TIM3
#define TIMER_CLK RCC_APB1Periph_TIM3
#define TIMER_IRQn TIM3_IRQn

/**
 * @brief  pwm波初始化
 * @param  无
 * @return 无
 */
void pwm_tim_init(void);

/**
 * @brief  红外接收输入捕获TIM初始化
 * @param  无
 * @return 无
 */
void ir_tim_init(void);

/**
 * @brief  计数定时器初始化
 * @param  无
 * @return 无 
 */
void timer_init(void);

/**
 * @brief  设置PWM输出通道2输出比较寄存器
 * @param  val 需要设置的值
 * @return 无 
 */
void pwm_setcompare2(uint16_t val);


/**
 * @brief  设置PWM输出通道3输出比较寄存器
 * @param  val 需要设置的值
 * @return 无 
 */
void pwm_setcompare3(uint16_t val);
#endif 
