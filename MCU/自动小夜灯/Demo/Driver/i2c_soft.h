#ifndef __I2C_SOFT_H__
#define __I2C_SOFT_H__

#include "stm32f10x.h"
#include "delay.h"

#define SOFT_I2C_PORT GPIOB
#define SOFT_I2C_SCL_PIN GPIO_Pin_6
#define SOFT_I2C_SDA_PIN GPIO_Pin_7
#define SOFT_I2C_CLK RCC_APB2Periph_GPIOB

#define SOFT_I2C_WRITE_BIT 0
#define SOFT_I2C_READ_BIT  1
#define SOFT_I2C_SCL_WRITE(x) GPIO_WriteBit(SOFT_I2C_PORT, SOFT_I2C_SCL_PIN, (BitAction)x)
#define SOFT_I2C_SDA_WRITE(x) GPIO_WriteBit(SOFT_I2C_PORT, SOFT_I2C_SDA_PIN, (BitAction)x)

/**
  *@brief  软件I2C初始化 
  *@param  无
  *@return 无
  */
void soft_i2c_init(void);

/**
  *@brief  实现软件I2C读功能，主要是用于对从机特定寄存器的读
  *@param  slave_addr 从机地址
  *@param  data 读的数据
  *@param  data_size 读的数据大小
  *@return 无
  */
void soft_i2c_read(uint8_t slave_addr, uint8_t *data, uint16_t data_size);

/**
  *@brief  实现软件I2C写功能，主要是用于对从机特定寄存器的写 
  *@param  slave_addr 从机地址
  *@param  data 写的数据
  *@param  data_size 写的数据大小
  *@return 无
  */
void soft_i2c_write(uint8_t slave_addr, const uint8_t *data, uint16_t data_size);

/**
  *@brief  实现软件I2C读写功能，主要是用于对从机特定寄存器的读写 
  *@param  slave_addr 从机地址
  *@param  write_data 写的数据
  *@param  write_data_size 写的数据大小
  *@param  read_data 读的数据
  *@param  read_data 读的数据大小
  *@return 无
  */
void soft_i2c_write_read(uint8_t slave_addr, const uint8_t *write_data, uint16_t write_data_size,uint8_t *read_data, uint16_t read_data_size);

#endif
