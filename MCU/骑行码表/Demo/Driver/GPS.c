#include "GPS.h"

/* GPS传递的数据结构体 */
static struct {
	float speed_kt;       /**< 速度，单位海里 */
	char bj_time_str[32]; /**< 时间 */
	char latitude[12];    /**< 维度 */
	char longitude[13];   /**< 经度 */
} gps_inform = {
	.speed_kt = 0.0,
	.bj_time_str = "24-01-01 00:00:00",
	.latitude = "0",
	.longitude = "0"
};

static char gprmc_buffer[GPRMC_BUFFER_SIZE];
static char rx_buffer[RX_BUFFER_SIZE];
static uint16_t rx_index = 0;
static uint8_t gprmc_receveid = 0;//是否接收的的标志位

/* UTC时间转换成北京时间 */
static void UTC_to_BJT(const char *utc_time, const char *utc_date, char *bj_time, char *bj_date)
{
	int hour = 0, minute = 0, second = 0, day = 0, month = 0, year = 0;
	int days_in_month[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	
	sscanf(utc_time, "%2d%2d%2d", &hour, &minute, &second);
	sscanf(utc_date, "%2d%2d%2d", &day, &month, &year);
	
	if ((year % 4 == 0 && year % 100 == 0) || (year % 400 == 0)) {
		days_in_month[1] = 29;
	}
	
	hour += 8;
	
	if (hour >= 24) {
		hour = 0;
		day += 1;
		if (day > days_in_month[month - 1]) {
			day = 1;
			month += 1;
			if (month > 12) {
				month = 1;
				year += 1;
			}
		}
	}
	
	snprintf(bj_date, 9, "%02d-%02d-%02d", year, month, day);
	snprintf(bj_time, 9, "%02d:%02d:%02d", day, hour, second);
}

/**
 * @brief  判断是否接收到数据
 * @param  无
 * @return 0表示未接受，1表示接收到数据
 */
int is_received_GPRMC(void)
{
	return gprmc_receveid;
}

/**
 * @brief  解析GPRMC数据
 * @param  无
 * @return 无
 */
void prase_GPRMC(void)
{
	/*发送示例
    *$GPRMC,060322.00,A,4003.520920,N,11620.220964,E,0.0354,010624,,,A*72
	*头，时间，状态，纬度(latitude)，方向，经度(longitude)，方向，速度，航向，日期，磁偏角，指示器，校验和
	*/ 
	int field_index = 0;
	int field_len = 0;
	const char *start = gprmc_buffer;
	const char *end = NULL;
	char utc_time[11] = {0}, utc_date[7] = {0}, speed_str[12] = {0};
	char bj_time[9] = {0}, bj_date[9] = {0};
	
	gprmc_receveid = 0;
	
	if (strncmp(gprmc_buffer, "$GPRMC", 6) == 0) {
		while ((end = strstr(start, ",")) != NULL) {
			field_len = (int)(end - start);
			
			switch (field_index) {
				case 1: //时间
					strncpy(utc_time, start, field_len);
					utc_time[sizeof(utc_time) - 1] = '\0';
					break;
				case 3: //纬度
					strncpy(gps_inform.latitude , start, field_len);
					gps_inform.latitude[sizeof(gps_inform.latitude) - 1] = '\0';
				case 5: //经度
					strncpy(gps_inform.longitude, start, field_len);
					gps_inform.longitude[sizeof(gps_inform.longitude) - 1] = '\0';
					break;
				case 7: //速度，单位节
					strncpy(speed_str, start, field_len);
					speed_str[sizeof(speed_str) - 1] = '\0';
					break;
				case 9: //日期
					strncpy(utc_date, start, field_len);
					utc_date[sizeof(utc_date) - 1] = '\0';
					break;
				default:
					break;
			}
			start = end + 1;
			field_index += 1;
		}
		printf("UTC TIME: %s\r\n", utc_time);
		printf("UTC DATE: %s\r\n", utc_date);
		
		UTC_to_BJT(utc_time, utc_date, bj_time, bj_date);
		
		sscanf(speed_str, "%f", &gps_inform.speed_kt);
		
		printf("BJ TIME: %s\r\n", bj_time);
		printf("BJ DATE: 20%s\r\n", bj_date);
		
		sprintf(gps_inform.bj_time_str, "%s %s", bj_date, bj_time);
		
		printf("LATITUDE: %s\r\n", gps_inform.latitude);
		printf("LONGITITUDE: %s\r\n", gps_inform.longitude);
		printf("----------------------------------------\r\n");
	}
}

/**
 * @brief  接收gps发送的数据到缓冲区
 * @param  rx_data 接收的数据
 * @return 无
 */
void gps_data_received(uint8_t rx_data)
{
	rx_buffer[rx_index++] = rx_data;
	
	if (rx_data == '\n') {
		rx_buffer[rx_index] = '\0';
		
		if (strncmp(rx_buffer, "$GPRMC", 6) == 0) {
			strncpy(gprmc_buffer, rx_buffer, sizeof(gprmc_buffer) - 1);
			gprmc_receveid = 1;
		}
		rx_index = 0;
	}
}

/**
 * @brief  获取时间字符接口
 * @param  无
 * @return 无 
 */
char* get_time_str(void)
{
	return gps_inform.bj_time_str;
}


/**
 * @brief  获取纬度接口
 * @param  无
 * @return 无 
 */
char* get_latitude(void)
{
	return gps_inform.latitude;
}

/**
 * @brief  获取经度接口
 * @param  无
 * @return 无 
 */
char* get_longitude(void)
{
	return gps_inform.longitude;
}

/**
 * @brief  获取海里速度接口
 * @param  无
 * @return 无 
 */
float get_speed_kt(void)
{
	return gps_inform.speed_kt;
}
