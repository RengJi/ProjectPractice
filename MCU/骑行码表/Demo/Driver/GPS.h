#ifndef __GPS_H
#define __GPS_H

#include "stm32f10x.h"                  // Device header
#include <string.h>
#include <stdio.h>


#define GPRMC_BUFFER_SIZE 128
#define RX_BUFFER_SIZE 1024

/**
 * @brief  判断是否接收到数据
 * @param  无
 * @return 0-代表未接收到完整数据;1-表示接收到完整数据
 */
int is_received_GPRMC(void);

/**
 * @brief  解析GPS发送的数据
 * @param  无
 * @return 无 
 */
void prase_GPRMC(void);

/**
 * @brief  预处理接收的数据
 * @param  无
 * @return 无 
 */
void gps_data_received(uint8_t rx_data);

/**
 * @brief  获取时间字符接口
 * @param  无
 * @return 无 
 */
char* get_time_str(void);


/**
 * @brief  获取纬度接口
 * @param  无
 * @return 无 
 */
char* get_latitude(void);

/**
 * @brief  获取经度接口
 * @param  无
 * @return 无 
 */
char* get_longitude(void);

/**
 * @brief  获取海里速度接口
 * @param  无
 * @return 无 
 */
float get_speed_kt(void);

#endif
