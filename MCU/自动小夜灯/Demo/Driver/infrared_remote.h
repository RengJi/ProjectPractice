#ifndef __INFRARED_REMOTE_H__
#define __INFRARED_REMOTE_H__

#include "stm32f10x.h"                  // Device header
#include <stdio.h>

#define REMOTE_USER_CODE_L 0x00
#define REMOTE_USER_CODE_H 0xFF
#define IR_GPIO_CLK  RCC_APB2Periph_GPIOA
#define IR_GPIO_PORT GPIOA
#define IR_GPIO_PIN  GPIO_Pin_8

/**
  *@brief  红外接收引脚初始化
  *@param  无
  *@return 无
  */
void ir_gpio_init(void);

/**
  *@brief  打印数组
  *@param  prefix 前置信息
  *@param  array 需要打印的数组
  *@param  size 需要打印的数组大小
  *@return 无
  */
void print_hex(const char *prefix, uint8_t *array, uint16_t size);

/**
  *@brief 完成解码操作
  *@param bit 当前的位数
  *@return 解码完成返回1，否则返回0
  */
int ir_decode_code(uint8_t bit);

/**
  *@brief 解码每一位的数值
  *@param low_level_duration 一个有效数据位的低电平持续时长，单位100us
  *@param high_level_duration 一个有效数据位的高电平持续时长，单位100us
  *@return 无
  */
void ir_decode_bit(uint8_t low_level_duration, uint8_t high_level_duration);

/**
  *@brief  每100us统计一次电平状态
  *@param  无
  *@return 无
  */
void ir_measure_bit_level_every_100us(void);

#endif
