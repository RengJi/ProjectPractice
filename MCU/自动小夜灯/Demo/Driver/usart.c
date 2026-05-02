#include "usart.h"
#include "delay.h"

/**
  *@brief  硬件串口1发送一字节
  *@param  byte 需要发送的字节
  *@return 无
  */
void usart1_send_byte(uint8_t byte)
{
	USART_SendData(USART1, byte);
	
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

/**
  *@brief  软件串口发送一字节
  *@param  byte 需要发送的字节
  *@return 无
  */
void serial_soft_send_byte(uint8_t byte)
{
	GPIO_ResetBits(SERIAL_SOFT_PORT, SERIAL_SOFT_TX_PIN);
	delay_us(104); /* 波特率9600算出来的 */
	for (uint8_t i = 0; i < 8; ++i) {
		if (byte & 0x01) {
			GPIO_SetBits(SERIAL_SOFT_PORT, SERIAL_SOFT_TX_PIN);
		} else {
			GPIO_ResetBits(SERIAL_SOFT_PORT, SERIAL_SOFT_TX_PIN);
		}
		delay_us(104);
		byte = byte >> 1;
	}
	GPIO_SetBits(SERIAL_SOFT_PORT, SERIAL_SOFT_TX_PIN);
	delay_us(104);
}

/**
  *@brief  硬件串口1发送数组
  *@param  hex_array 需要发送的数组
  *@param  size 需要发送的数组大小
  *@return 无
  */
void usart1_send_array(uint8_t *hex_array, uint16_t size)
{
	while (size--) {
		usart1_send_byte(*hex_array++);
	}
}

/**
  *@brief  软件串口发送数组
  *@param  hex_array 需要发送的数组
  *@param  size 需要发送的数组大小
  *@return 无
  */
void serial_soft_send_array(uint8_t *hex_array, uint8_t size)
{
	while (size--) {
		serial_soft_send_byte(*hex_array++);
	}
}


/**
  *@brief  硬件串口1发送字符串
  *@param  str 需要发送的字符串
  *@return 无
  */
void usart1_send_string(char *str)
{
	while (*str) {
		usart1_send_byte(*str++);
	}
}

/**
  *@brief  软件串口发送字符串
  *@param  str 需要发送的字符串
  *@return 无
  */
void serial_soft_send_string(char *str)
{
	while (*str) {
		serial_soft_send_byte(*str++);
	}
}

/**
  *@brief  串口1初始化
  *@param  无
  *@return 无
  */
void usart1_init(void)
{
	RCC_APB2PeriphClockCmd(USART_CLK, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = USART_TX_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(USART_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = USART_RX_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(USART_PORT, &GPIO_InitStructure);
	
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 115200;
	USART_InitStructure.USART_HardwareFlowControl = DISABLE;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStructure);
	
	
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
	
	USART_Cmd(USART1, ENABLE);
}

/**
  *@brief  软件串口初始化
  *@param  无
  *@return 无
  */
void serial_soft_init(void)
{
	RCC_APB2PeriphClockCmd(SERIAL_SOFT_CLK, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = SERIAL_SOFT_TX_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(SERIAL_SOFT_PORT, &GPIO_InitStructure);
	GPIO_SetBits(SERIAL_SOFT_PORT, SERIAL_SOFT_TX_PIN);
}

/* 输出流重定向 */
int fputc(int ch, FILE *f)
{
	usart1_send_byte(ch);
	return ch;
}

/**
  *@brief  硬件串口3发送一字节
  *@param  byte 需要发送的字节
  *@return 无
  */
void usart3_send_byte(uint8_t byte)
{
	USART_SendData(USART3, byte);
	
	while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
}

/**
  *@brief  硬件串口3发送数组
  *@param  hex_array 需要发送的数组
  *@param  size 需要发送的数组大小
  *@return 无
  */
void usart3_send_array(uint8_t *hex_array, uint8_t size)
{
	while (size--) {
		usart3_send_byte(*hex_array++);
	}
}

/**
  *@brief  硬件串口3发送字符串
  *@param  str 需要发送的字符串
  *@return 无
  */
void usart3_send_string(char *str)
{
	while (*str) {
		usart3_send_byte(*str++);
	}
}

/**
  *@brief  串口3初始化
  *@param  无
  *@return 无
  */
void usart3_init(void)
{
	RCC_APB1PeriphClockCmd(USART3_CLK, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Pin = USART3_TX_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(USART3_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStructure.GPIO_Pin = USART3_RX_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(USART3_PORT, &GPIO_InitStructure);
	
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
	
	USART_InitTypeDef USART_InitStructure;
	USART_InitStructure.USART_BaudRate = 9600;
	USART_InitStructure.USART_HardwareFlowControl = DISABLE;
	USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART3, &USART_InitStructure);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
	
	USART_Cmd(USART3, ENABLE);
}
