#include "tim.h"
#include "motor.h"

/**
 * @brief  电机刹车
 * @param  无
 * @return 无
 */
void motor_break(void)
{
	pwm_setcompare2(100);
	pwm_setcompare3(100);
}


/**
 * @brief  电机停止
 * @param  无
 * @return 无
 */
void motor_standby(void)
{
	pwm_setcompare2(0);
	pwm_setcompare3(0);
}

/**
 * @brief  设置电机速度
 * @param  direction 电机方向
 * @param  speed 电机速度 1-100
 * @return 无
 */
void motor_speed(uint8_t direction, uint8_t speed)
{
	if (direction == 0) {
		pwm_setcompare2(speed);
		pwm_setcompare3(0);
	} else {
		pwm_setcompare2(0);
		pwm_setcompare3(speed);
	}
}
