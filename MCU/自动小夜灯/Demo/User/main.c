#include <stdio.h>
#include "stm32f10x.h"                  // Device header
#include "tim.h"
#include "delay.h"
#include "sound.h"
#include "light.h"
#include "relay.h"
#include "usart.h"
#include "buzzer.h"
#include "at24cxx.h"
#include "body_detect.h"
#include "infrared_remote.h"

#define KEY_CODE_SWITCH 0x45    //开关键值
#define KEY_CODE_MODE   0x46    //模式切换键值

#define SWITCH_STATUS_OFF 0x00  //关闭状态
#define SWITCH_STATUS_ON  0x01  //开启状态

#define MODE_STATUS_DETECT 0x00 //感应模式
#define MODE_STATUS_KEEP   0x01 //常量模式
 
#define SWITCH_STATUS_ADDR 0x00 //开关状态存储地址
#define MODE_STATUS_ADDR   0x01 //模式状态存储地址
#define INTI_STATUS_ADDR   0x02 //初始化标志存储地址

#define SWITCH_INIT_STATUS 0x01 //默认开启状态
#define MODE_INIT_STATUS   0x00 //默认感应模式
#define INIT_FLAG          0xA5 //是否第一次运行标志

extern int is_sound_loud;
extern int is_body_pass;
extern uint8_t key_code;

int main(void)
{
	/* 设置中断向量控制器 */
	NVIC_SetPriorityGrouping(NVIC_PriorityGroup_2);
	
	/* 声明变量 */
	uint16_t light_val = 0;
	uint8_t init_flag = 0x00, switch_status = 0x00, mode = 0x00; 
	
	/* 初始化外设 */
	sound_init();
	buzzer_init();
	usart1_init();
	ir_tim_init();
	ir_gpio_init();
	soft_i2c_init();
	relay_pin_init();
	body_detect_init();
	light_ao_gpio_init();
	
	/* 判断是否第一次运行 */
	at24cxx_read_bytes(INTI_STATUS_ADDR, &init_flag, 1);
	if (init_flag != INIT_FLAG) {
		init_flag = INIT_FLAG;
		switch_status = SWITCH_INIT_STATUS;
		mode = MODE_INIT_STATUS;
		at24cxx_write_byte(INTI_STATUS_ADDR, &init_flag);
		at24cxx_write_byte(SWITCH_STATUS_ADDR, &switch_status);
		at24cxx_write_byte(MODE_STATUS_ADDR, &mode);
		printf("systerm first init ! switch status is %d , mode status is %d\r\n", switch_status, mode);
	} else {
		at24cxx_read_bytes(SWITCH_STATUS_ADDR, &switch_status, 1);
		at24cxx_read_bytes(MODE_STATUS_ADDR, &mode, 1);
		printf("systerm init ! switch status is %d , mode status is %d\r\n", switch_status, mode);
	}
	
	
	while (1) {
		delay_ms(500);
		
		/* 开关和切换模式 */
		if (key_code != 0x00) {
			if (key_code == KEY_CODE_SWITCH) {
				switch_status = (switch_status == SWITCH_STATUS_ON) ? SWITCH_STATUS_OFF : SWITCH_STATUS_ON;
				at24cxx_write_byte(SWITCH_STATUS_ADDR, &switch_status);
				printf("switch change, current status is %d\r\n", switch_status);
				buzzer(1);
			} else if (key_code == KEY_CODE_MODE) {
				mode = (mode == MODE_STATUS_DETECT) ? MODE_STATUS_KEEP : MODE_STATUS_DETECT;
				at24cxx_write_byte(MODE_STATUS_ADDR, &mode);
				printf("mode change, current mode is %d\r\n", mode);
				buzzer(1);
			} else {
				printf("unsupprot key\r\n");
				buzzer(3);
			}
			key_code = 0x00;	
		}
		
		/* 夜灯行为 */
		if (switch_status == SWITCH_STATUS_ON) {  //小夜灯开启
			light_val = adc_get_lightval();
			printf("light_val is %d\r\n", light_val);
			
			if (light_val > 2000) {               //光照环境较暗，启用小夜灯
				if (mode == MODE_STATUS_DETECT) { //感应模式
						/* 有人经过或者发出声音 */
						if (is_body_pass == 1 || is_sound_loud == 1) {
							printf("is_body_pass: %d, is_sound_loud: %d\r\n", is_body_pass, is_sound_loud);
							relay_on();
							delay_s(10);          //照明时间为10s
							relay_off();
							is_body_pass = is_sound_loud = 0; 
						}
				} else {                          //常亮模式
					relay_on();
				}
			} else {                              //光照较强
				relay_off();
				is_body_pass = is_sound_loud = 0; 
			}
		} else {
			/* 小夜灯关闭，什么都不做 */
		}
	}
}

