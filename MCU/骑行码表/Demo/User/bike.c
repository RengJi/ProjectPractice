#include "bike.h"

static struct bike_inform bike_data = {
	.total_sec = 0,                  /**< 总的时间，单位：s */
	.total_distance_m = 0.0,         /**< 总的距离，单位：m */
	.speed_kmh = 0.0,                /**< 实时速度，单位：km/h */
	.max_speed_kmh = 0.0,            /**< 最大速度，单位：km/h */
	.running_status = IDLE,          /**< 运行状态 */
	.screen_number = SCREEN_WELCOME  /**< 屏显状态 */
};


/**
 * @brief  清空保存记录属性的数据
 * @param  无
 * @return 无
 */
static void reset_data(void) 
{  
	bike_data.total_sec = 0;          //总的时间，单位：s
	bike_data.total_distance_m = 0.0; //总的距离，单位：m
	bike_data.max_speed_kmh = 0.0;    //最大速度，单位：km/h 
}

/**
 * @brief  将一个秒数转换成常用格式时间
 * @param  sec 需要转换的秒数
 * @param  formatted_time 转换完毕之后的时间
 * @return 无
 */
static void second_format(uint32_t total_sec, char *formatted_time)
{
	uint8_t hour = 0, min = 0, sec = 0;
	
	hour = total_sec / 3600;
	total_sec %= 3600;
	min = total_sec / 60;
	sec = total_sec % 60;
	sprintf(formatted_time, "%02d:%02d:%02d", hour, min, sec);
}

/**
 * @brief  开机界面显示
 * @param  无
 * @return 无
 */
static void screen_welcome_show(void)
{
	char OLED_Show[32];
	strcpy(OLED_Show, get_time_str());
	
	OLED_ShowString(1, 1, &OLED_Show[3]);
	OLED_ShowString(2, 1, "Welcome to Use");
	OLED_ShowString(3, 1, "FS Bike Watch!");
}

/**
 * @brief  信息显示页1
 * @param  无
 * @return 无
 */
static void screen_1_show(void)
{
	char OLED_Show[32] = {0};
	
	/* 显示时间 */
	strcpy(OLED_Show, get_time_str());
	OLED_ShowString(1, 1, &OLED_Show[3]);
	
	/* 显示实时速度 */
	sprintf(OLED_Show, "%.02f km/h", bike_data.speed_kmh);
	OLED_ShowString(2, 1, OLED_Show);
	
	/* 显示骑行时长 */
	second_format(bike_data.total_sec, OLED_Show);
	OLED_ShowString(3, 1, OLED_Show);
	
	/* 显示骑行距离 */
	sprintf(OLED_Show, "%.02f km", bike_data.total_distance_m / 1000.0);
	OLED_ShowString(4, 1, OLED_Show);
	
	/* 显示当前状态 */
	if (bike_data.running_status == RUNNING) {
		OLED_ShowString(4, 11, "go!");
	} else if (bike_data.running_status == STOP) {
		OLED_ShowString(4, 11, "stop");
	}
}

/**
 * @brief  信息显示页2
 * @param  无
 * @return 无
 */
static void screen_2_show(void)
{
	char OLED_Show[32] = {0};
	float total_distance_km = bike_data.total_distance_m / 1000.0;
	float humi, temp, hours;
	float kcal;
	
	/* 显示最高时速 */
	sprintf(OLED_Show, "%.02f km/h", bike_data.max_speed_kmh);
	OLED_ShowString(1, 1, OLED_Show);
	
	/* 显示平均时速 */
	hours = (float)bike_data.total_sec / 3600.0;
	sprintf(OLED_Show, "%.02f km/h", total_distance_km / hours);
	OLED_ShowString(2, 1, OLED_Show);
	
	/* 显示消耗的卡路里 */
	kcal = (float)WEIGHT_KG * total_distance_km * WEIGHT_KG;
	sprintf(OLED_Show, "%.02f kcal", kcal);
	OLED_ShowString(3, 1, OLED_Show);
	
	/* 显示温湿度 */
	aht10_read(&humi, &temp);
	sprintf(OLED_Show, "%.02fC %.02f%%", temp, humi);
	OLED_ShowString(4, 1, OLED_Show);
	
	/* 显示当前状态 */
	if (bike_data.running_status == RUNNING) {
		OLED_ShowString(1, 13, "go!");
	} else if (bike_data.running_status == STOP) {
		OLED_ShowString(1, 13, "stop");
	}
}

/**
 * @brief  根据状态显示数据
 * @param  无
 * @return 无
 */
static void watch_screen_show(void)
{
	if (bike_data.screen_number == SCREEN_WELCOME) {
		screen_welcome_show();
	} else if (bike_data.screen_number == SCREEN_1) {
		screen_1_show();
	} else if (bike_data.screen_number == SCREEN_2) {
		screen_2_show();
	}
}

/**
 * @brief  按键1切换运行状态功能
 * @param  无
 * @return 无
 */
void running_status_change(void)
{
	if (bike_data.running_status == IDLE) {
		bike_data.screen_number = SCREEN_1;  //屏幕开始显示信息页1
		bike_data.running_status = RUNNING; //状态切换为运行
		
		OLED_Clear();
		
	} else if (bike_data.running_status == RUNNING) {
		bike_data.running_status = STOP;
	} else {    //关机
		OLED_Clear();
		bike_data.running_status = IDLE;
		bike_data.screen_number = SCREEN_WELCOME;
		reset_data();
	}
	printf("current watch_flag is %d\r\n", bike_data.running_status);
}

/**
 * @brief  按键2切换显示信息
 * @param  无
 * @return 无
 */
void screen_show_change(void)
{
	if (bike_data.screen_number == SCREEN_1) {
		bike_data.screen_number = SCREEN_2;
		OLED_Clear();
	} else if (bike_data.screen_number == SCREEN_2) {
		bike_data.screen_number = SCREEN_1;
		OLED_Clear();
	}
	printf("current screen_flag is %d\r\n", bike_data.screen_number);
}

/**
 * @brief  码表数据更新
 * @param  无
 * @return 无
 */
void bike_data_update(void)
{
	if (bike_data.running_status == RUNNING) {
		bike_data.speed_kmh = get_speed_kt() * 1.852;
		bike_data.total_distance_m += get_speed_kt() * 0.514 * 1.0;
		bike_data.total_sec++;
		if (bike_data.speed_kmh > bike_data.max_speed_kmh) 
			bike_data.max_speed_kmh = bike_data.speed_kmh;
	}
	
	watch_screen_show();
}

