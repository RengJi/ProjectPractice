#ifndef __AHT10_H__
#define __AHT10_H__

#include <stdbool.h>
#include "i2c_soft.h"
#include "delay.h"
#include "stm32f10x.h"                  // Device header

/**
 * @brief  使用AHT11模块获取温度湿度
 * @param  *humi湿度
 * @param  *temp温度
 * @return true获取成功，false获取失败
 */
bool aht10_read(float *humi, float *temp);

#endif
