#include "infrared_remote.h"
#include <string.h>

uint8_t key_code = 0; //键码，全局变量

/**
  *@brief  打印数组
  *@param  prefix 前置信息
  *@param  array 需要打印的数组
  *@param  size 需要打印的数组大小
  *@return 无
  */
void print_hex(const char *prefix, uint8_t *array, uint16_t size)
{
	printf("%s\r\n", prefix);
	for (uint16_t i = 0; i < size; ++i) {
		printf("0x%02x ", array[i]);
		if (i % 16 == 15) 
			printf("\r\n");
	}
}

/**
  *@brief 完成解码操作
  *@param bit 当前的位数
  *@return 解码完成返回1，否则返回0
  */
int ir_decode_code(uint8_t bit)
{
	static uint8_t ir_data[4];    //存放一个完整码，16位用户码和8位码位及其反码
	static uint8_t bit_number = 0;
	
	if (bit_number < 8) 
		ir_data[0] |= (bit << (bit_number % 8));
	else if (bit_number < 16)
		ir_data[1] |= (bit << (bit_number % 8));
	else if (bit_number < 24)
		ir_data[2] |= (bit << (bit_number % 8));
	else 
		ir_data[3] |= (bit << (bit_number % 8));
	
	bit_number++;
	if (bit_number == 32) {
		bit_number = 0;
		if ((ir_data[0] == REMOTE_USER_CODE_L) && (ir_data[1] == REMOTE_USER_CODE_H)) {
			if (ir_data[2] == (uint8_t)~ir_data[3]) {
				key_code = ir_data[2];
			}
		}
		memset(ir_data, 0, sizeof(ir_data));
		return 1;
	}
	return 0;
}

/**
  *@brief 解码每一位的数值
  *@param low_level_duration 一个有效数据位的低电平持续时长，单位100us
  *@param high_level_duration 一个有效数据位的高电平持续时长，单位100us
  *@return 无
  */
void ir_decode_bit(uint8_t low_level_duration, uint8_t high_level_duration)
{
	uint8_t bit_value;                 //位值
	static uint8_t rsv_start_data = 0; //起始信号接收成功标志位
	
	switch(rsv_start_data) {
		case 0:
			if ((low_level_duration > 85) && (low_level_duration < 95) && (high_level_duration > 40) && (high_level_duration < 50))
				rsv_start_data = 1;
			break;
		case 1:
			if ((low_level_duration > 4) && (low_level_duration < 8) && (high_level_duration > 4) && (high_level_duration < 8)) {
				bit_value = 0;
			} else if (((low_level_duration > 4) && (low_level_duration < 8) && (high_level_duration > 15) && (high_level_duration < 19))) {
				bit_value = 1;
			} else {
				rsv_start_data = 0;
			}
			
			if (ir_decode_code(bit_value))
				rsv_start_data = 0;        //完成一次完整解码，清除标志位
			break;
		default:
			break;
	}
}

