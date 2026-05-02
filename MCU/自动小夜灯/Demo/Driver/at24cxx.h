#ifndef __AT24CXX_H__
#define __AT24CXX_H__

#include "stm32f10x.h"                  // Device header
#include "i2c_soft.h"
#include "usart.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

#define AT24CXX_ADDR (0x50 << 1)
#define AT24C64_PAGE_SIZE 32

/**@brief  读取当前地址指针的地址内容
  *@param  data 读取到的内容
  *@retval 无
  */
void at24cxx_read_current(uint8_t *data);

/**@brief  顺序读取指定个数当前地址指针所以的内容
  *@param  data 读取到的内容组成的数组
  *@param  data_size 读取到的内容个数
  *@retval 无
  */
void at24cxx_read_seq(uint8_t *data, uint16_t data_size);

/**@brief  顺序读取指定位置指定个数的内容
  *@param  addr 指定读取的地址
  *@param  data 读取的内容组成的数组  
  *@param  data_size 读取到的内容个数
  *@retval 无
  */
void at24cxx_read_bytes(uint16_t addr, uint8_t *data, uint16_t data_size);


/**@brief  对EPROM某个地址写入
  *@param  addr 需要写入的地址
  *@param  data 写入的数据
  *@retval 无
  */
void at24cxx_write_byte(uint16_t addr, uint8_t *data);

/**@brief  对EPROM特定地址开始写入多个字节
  *@param  addr 需要写入的地址
  *@param  data 写入的数据
  *@param  data_size 写入数据的个数
  *@retval 无
  */
void at24cxx_write_bytes(uint16_t addr, uint8_t *data, uint16_t data_size);

/* 功能测试程序 */
void at24cxx_test(void);

#endif
