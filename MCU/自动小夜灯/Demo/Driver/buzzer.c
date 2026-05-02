#include "buzzer.h"

/**
 * @brief  蜂鸣器引脚初始化
 * @param  无
 * @return 无
 */
void buzzer_init(void)
{
	RCC_APB2PeriphClockCmd(BUZZER_CLK, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);
	
	GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

/**
 * @brief  使蜂鸣器响指定秒数
 * @param  sec 需要响的秒数
 * @return 无
 */
void buzzer(uint8_t sec)
{
	GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
	delay_s(sec);
	GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

