#ifndef __USART_H__
#define __USART_H__

#include "stm32f10x.h"                  // Device header
#include <stdio.h>

#define USART_PORT GPIOA
#define USART_TX_PIN GPIO_Pin_9
#define USART_RX_PIN GPIO_Pin_10
#define USART_CLK (RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1)

#define SERIAL_SOFT_PORT GPIOA
#define SERIAL_SOFT_TX_PIN GPIO_Pin_9
#define SERIAL_SOFT_RX_PIN GPIO_Pin_10
#define SERIAL_SOFT_CLK RCC_APB2Periph_GPIOA

#define USART3_PORT GPIOB
#define USART3_TX_PIN GPIO_Pin_10
#define USART3_RX_PIN GPIO_Pin_11
#define USART3_CLK RCC_APB1Periph_USART3

/**
  *@brief  硬件串口1发送一字节
  *@param  byte 需要发送的字节
  *@return 无
  */
void usart1_send_byte(uint8_t byte);

/**
  *@brief  软件串口发送一字节
  *@param  byte 需要发送的字节
  *@return 无
  */
void serial_soft_send_byte(uint8_t byte);

/**
  *@brief  硬件串口1发送数组
  *@param  hex_array 需要发送的数组
  *@param  size 需要发送的数组大小
  *@return 无
  */
void usart1_send_array(uint8_t *hex_array, uint16_t size);

/**
  *@brief  软件串口发送数组
  *@param  hex_array 需要发送的数组
  *@param  size 需要发送的数组大小
  *@return 无
  */
void serial_soft_send_array(uint8_t *hex_array, uint8_t size);

/**
  *@brief  硬件串口1发送字符串
  *@param  str 需要发送的字符串
  *@return 无
  */
void usart1_send_string(char *str);

/**
  *@brief  软件串口发送字符串
  *@param  str 需要发送的字符串
  *@return 无
  */
void serial_soft_send_string(char *str);

/**
  *@brief  串口1初始化
  *@param  无
  *@return 无
  */
void usart1_init(void);

/**
  *@brief  软件串口初始化
  *@param  无
  *@return 无
  */
void serial_soft_init(void);

/**
  *@brief  硬件串口3发送一字节
  *@param  byte 需要发送的字节
  *@return 无
  */
void usart3_send_byte(uint8_t byte);

/**
  *@brief  硬件串口3发送数组
  *@param  hex_array 需要发送的数组
  *@param  size 需要发送的数组大小
  *@return 无
  */
void usart3_send_array(uint8_t *hex_array, uint8_t size);

/**
  *@brief  硬件串口3发送字符串
  *@param  str 需要发送的字符串
  *@return 无
  */
void usart3_send_string(char *str);

/**
  *@brief  串口3初始化
  *@param  无
  *@return 无
  */
void usart3_init(void);

#endif
