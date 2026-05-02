#include "relay.h"

/**
  *@brief  继电器引脚初始化
  *@param  无
  *@return 无
  */
void relay_pin_init(void)
{
	RCC_APB2PeriphClockCmd(RELAY_CLK, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = RELAY_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(RELAY_PORT, &GPIO_InitStructure);
	
	relay_off();
}

/**
  *@brief  关闭继电器
  *@param  无
  *@return 无
  */
void relay_off(void)
{
	GPIO_SetBits(RELAY_PORT, RELAY_PIN);
}

/**
  *@brief  打开继电器
  *@param  无
  *@return 无
  */
void relay_on(void)
{
	GPIO_ResetBits(RELAY_PORT, RELAY_PIN);
}

/**
  *@brief  切换继电器状态
  *@param  无
  *@return 无
  */
void relay_toggle(void)
{
	if (GPIO_ReadInputDataBit(RELAY_PORT, RELAY_PIN) == Bit_SET) {
		relay_on();
	} else {
		relay_off();
	}
}
