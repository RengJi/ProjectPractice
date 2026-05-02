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

void usart1_send_byte(uint8_t byte);
void serial_soft_send_byte(uint8_t byte);
void usart1_send_array(uint8_t *hex_array, uint16_t size);
void serial_soft_send_array(uint8_t *hex_array, uint8_t size);
void usart1_send_string(char *str);
void serial_soft_send_string(char *str);
void usart1_init(void);
void serial_soft_init(void);
void usart3_send_byte(uint8_t byte);
void usart3_send_array(uint8_t *hex_array, uint8_t size);
void usart3_send_string(char *str);
void usart3_init(void);

#endif
