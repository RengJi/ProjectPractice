#include "aht10.h"

#define AHT10_ADDRESS (0x38 << 1)

static void aht10_reset(void)
{
	uint8_t reset = 0xBA;
	soft_i2c_write(AHT10_ADDRESS, &reset, 1);
	delay_ms(20);//根据手册，重启延时不少于17ms
}


/**
 * @brief  使用AHT11模块获取温度湿度
 * @param  *humi湿度
 * @param  *temp温度
 * @return true获取成功，false获取失败
 */
bool aht10_read(float *humi, float *temp)
{
	uint8_t humi_temp[6] = {0};
	uint8_t measure[3] ={0xAC, 0x33, 0x00};
	
	soft_i2c_write(AHT10_ADDRESS, measure, 3);
	delay_ms(100); //根据手册至少延时75ms
	
	soft_i2c_read(AHT10_ADDRESS, humi_temp, 6);
	
	if ((humi_temp[0] & 0x08) == 0) { //传感器未校准
		aht10_reset();
		return false;
	} else {
		uint32_t SRH = (humi_temp[1] << 12) | (humi_temp[2] << 4)| (humi_temp[3] >> 4);
		uint32_t ST = ((humi_temp[3] & 0x0f) << 16) | (humi_temp[4] << 8) | humi_temp[5];
		
		*humi = (SRH * 100.0) / 1024.0 / 1024.0;
		*temp = (ST * 200.0) / 1024.0 / 1024.0 - 50.0;
		return true;
	}
}
