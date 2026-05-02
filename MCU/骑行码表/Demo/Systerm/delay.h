#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"                  // Device header

/**
  *@brief  us级别延时
  *@param  us 需要延时的微秒数
  *@return 无
  */
void delay_us(uint32_t us);

/**
  *@brief  ms级别延时
  *@param  ms 需要延时的毫秒数
  *@return 无
  */
void delay_ms(uint32_t ms);

/**
  *@brief  s级别延时
  *@param  s 需要延时的秒数
  *@return 无
  */
void delay_s (uint32_t s);

#endif
