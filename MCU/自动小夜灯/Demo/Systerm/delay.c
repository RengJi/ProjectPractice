#include "delay.h"

/**
  *@brief  us级别延时
  *@param  us 需要延时的微秒数
  *@return 无
  */
void delay_us(uint32_t us)
{
	SysTick->LOAD = 72 * us;
	SysTick->VAL = 0x00;
	SysTick->CTRL = 0x00000005; /* 设置时钟源为HCLK */
	while (!(SysTick->CTRL & 0x00010000)); /* 等待计数器减到零 */
	SysTick->CTRL = 0x00000004; /* 关闭时钟源 */
}

/**
  *@brief  ms级别延时
  *@param  ms 需要延时的毫秒数
  *@return 无
  */
void delay_ms(uint32_t ms)
{
	while (ms--) {
		delay_us(1000);
	}
}

/**
  *@brief  s级别延时
  *@param  s 需要延时的秒数
  *@return 无
  */
void delay_s (uint32_t s)
{
	while(s--) {
		delay_ms(1000);
	}
}
