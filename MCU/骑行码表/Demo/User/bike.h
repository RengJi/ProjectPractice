#ifndef __BIKE_H__
#define __BIKE_H__

#include "stm32f10x.h"                  // Device header
#include "GPS.h"
#include "OLED.h"
#include "aht10.h"
#include <stdio.h>

#define WEIGHT_KG 70        //默认体重，用于计算卡路里
#define WEIGHT_FACTOR 0.5   //默认体重系数

/* 运行状态 */
enum RUNNING_STATUS {
	IDLE = 0,
	RUNNING,
	STOP
};

/* 屏显编号 */
enum SCREEN_NUMBER {
	SCREEN_WELCOME,
	SCREEN_1,
	SCREEN_2
};

/* 骑行码表信息 */
struct bike_inform {
	uint32_t total_sec;                 /**< 总的时间，单位：s */
	float total_distance_m;             /**< 总的距离，单位：m */
	float speed_kmh;                    /**< 实时速度，单位：km/h */
	float max_speed_kmh;                /**< 最大速度，单位：km/h */
	enum RUNNING_STATUS running_status; /**< 运行状态 */
	enum SCREEN_NUMBER screen_number;   /**< 屏显状态 */
};

/**
 * @brief  按键1切换运行状态功能
 * @param  无
 * @return 无
 */
void running_status_change(void);

/**
 * @brief  按键2切换显示信息
 * @param  无
 * @return 无
 */
void screen_show_change(void);

/**
 * @brief  码表数据更新
 * @param  无
 * @return 无
 */
void bike_data_update(void);

#endif
