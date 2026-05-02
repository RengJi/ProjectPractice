#ifndef __MOTOR_H__
#define __MOTOR_H__

#include "stm32f10x.h"                  // Device header

/**
 * @brief  电机刹车
 * @param  无
 * @return 无
 */
void motor_break(void);

/**
 * @brief  电机停止
 * @param  无
 * @return 无
 */
void motor_standby(void);
/**
 * @brief  设置电机速度
 * @param  direction 电机方向
 * @param  speed 电机速度 1-100
 * @return 无
 */
void motor_speed(uint8_t direction, uint8_t speed);

#endif
