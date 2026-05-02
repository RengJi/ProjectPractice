#include <stdio.h>
#include "stm32f10x.h"                  // Device header
#include "tim.h"
#include "usart.h"
#include "i2c_soft.h"
#include "GPS.h"
#include "OLED.h"
#include "aht10.h"
#include "key.h"
#include "bike.h"

int main(void)
{
	/* 设置中断分组 */
	NVIC_SetPriorityGrouping(NVIC_PriorityGroup_2);
	
	/* 外设初始化 */
	timer_init();
	soft_i2c_init();
	usart1_init();
	usart3_init();
	OLED_Init();
	key_init();
	
	printf("systerm init\r\n");
	
	while (1) {
		/* 按键1切换运行状态 */
		if (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == Bit_RESET) {
			delay_ms(20);     //前置消抖
			if (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == Bit_RESET) {
				while (GPIO_ReadInputDataBit(KEY1_PORT, KEY1_PIN) == Bit_RESET);
				delay_ms(20); //后置消抖
				
				/* 切换运行状态 */
				running_status_change();
			}
		}
		
		/* 按键2切换显示 */
		if (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == Bit_RESET) {
			delay_ms(20);     //前置消抖
			if (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == Bit_RESET) {
				while (GPIO_ReadInputDataBit(KEY2_PORT, KEY2_PIN) == Bit_RESET);
				delay_ms(20); //后置消抖
				
				/* 切换显示信息 */
				screen_show_change();
			}
		}
		
		/* 如果接收到完整数据，则解析数据 */
		if (is_received_GPRMC()) {
			prase_GPRMC();
		}
		
		if ((get_sys_ms() % 1000) == 0) { //1s处理一次
			bike_data_update();
		}
		
	}
}

