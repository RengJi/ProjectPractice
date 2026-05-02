#include <stdio.h>
#include "stm32f10x.h"                  // Device header
#include "tim.h"
#include "usart.h"
#include "delay.h"
#include "buzzer.h"
#include "motor.h"
#include "infrared_remote.h"

#define KEY_CODE_SPEED_DOWN     0x45
#define KEY_CODE_SWITCH         0x46
#define KEY_CODE_SPEED_UP       0x47
#define KEY_CODE_POWER_OFF_TIME 0x44

extern uint8_t key_code;
int32_t time_set = 0;

int main(void)
{
	/* 设置中断向量控制器 */
	NVIC_SetPriorityGrouping(NVIC_PriorityGroup_2);
	
	/* 申明变量 */
	uint8_t is_running = 0;
	uint8_t min_set = 0;
	uint8_t speed = 0;
	uint8_t dir = 1;
	
	/* 外设初始化 */
	usart1_init();
	pwm_tim_init();
	ir_tim_init();
	timer_init();
	buzzer_init();
	
	printf("systerm init!\r\n");
	
	while (1) {
		if (key_code != 0x00) {
			switch (key_code) {
				case KEY_CODE_SWITCH:
					if (is_running == 0) {  //当前停止
						is_running = 1;
						speed = 50;
						printf("turn on!\r\n");
					} else {                //当前在运行
						is_running = 0;
						speed = 0;
						/* 取消定时关闭 */
						min_set = 0;      
						time_set = -1;      //设置为0的话会多次无效执行定时检测模块
					}
					break;
				case KEY_CODE_SPEED_DOWN:
					if (is_running && speed > 50)
						speed -= 10;
					buzzer(1);
					break;
				case KEY_CODE_SPEED_UP:
					if (is_running && speed < 80)
						speed += 10;
					buzzer(1);
					break;
				case KEY_CODE_POWER_OFF_TIME:
					if (is_running) {
						if (time_set == 120) { //如果定时达到120S
							min_set = 0;
							time_set = -1;
						} else {
							min_set += 30;
							time_set = min_set * 60;
							printf("time set is %d S\r\n", time_set);
						}
					}
					break;
				default:
					printf("unsupport key_code\r\n");
					break;
			}
			key_code = 0x00;
			
			motor_speed(dir, speed);
			printf("current status : speed-%d, dir-%d, time_set-%d\r\n", speed, dir, time_set);
		}
		
		/* 计时处理器定时完毕处理 */
		if (time_set == 0) { //倒计时完毕
			time_set = -1;   //关闭定时功能
			speed = 0;
			is_running = 0;
			motor_standby(); //关闭电机
		}
	}
}

