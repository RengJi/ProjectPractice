#include "key.h"

/** 
 * @brief  初始化按键
 * @param  无
 * @return 无
 */
void key_init(void)
{
	RCC_APB2PeriphClockCmd(KEY_CLK, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_InitStructure.GPIO_Pin = KEY1_PIN;
	GPIO_Init(KEY1_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = KEY2_PIN;
	GPIO_Init(KEY2_PORT, &GPIO_InitStructure);
}

