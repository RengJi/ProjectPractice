#include "light.h"

/**
  *@brief  初始化外设
  *@param  无
  *@return 无
  */
void light_ao_gpio_init(void)
{
	/* 开启时钟 */
	RCC_APB2PeriphClockCmd(LIGHT_CLK, ENABLE);
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);                                 //设置ADC的预分频系数
	
	/* 初始化GPIO */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = LIGHT_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(LIGHT_PORT, &GPIO_InitStructure);
	
	/* 初始化ADC */
	ADC_InitTypeDef ADC_InitStructure;
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;                  //开启连续转换
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;              //转化的数据右对齐
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; //关闭外部触发
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;                  //独立模式
	ADC_InitStructure.ADC_NbrOfChannel = ADC_Channel_1;                 //转换通道1
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;                       //关闭扫描模式
	ADC_Init(ADC1, &ADC_InitStructure);
	
	ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5); //设置采样时间为55.5个时钟周期
	
	ADC_Cmd(ADC1, ENABLE);            	//开启ADC转换
	
	/* 校准ADC */
	ADC_ResetCalibration(ADC1);                         //重置ADC校准寄存器
	while (ADC_GetResetCalibrationStatus(ADC1) == SET); //等待重置完成
	ADC_StartCalibration(ADC1);                         //ADC校准
	while (ADC_GetCalibrationStatus(ADC1) == SET);      //等待校准完成
}

/**
  *@brief  获取光强数据
  *@param  无
  *@return ADC获取到的光强
  */
uint16_t adc_get_lightval(void)
{
	ADC_SoftwareStartConvCmd(ADC1, ENABLE); //软件触发转换
	
	while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET); //等待转换完成
	return ADC_GetConversionValue(ADC1);
}
