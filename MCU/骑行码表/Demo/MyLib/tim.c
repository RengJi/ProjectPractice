#include "tim.h"

static uint32_t sys_ms = 0; //启动后经过的时间

/**
 * @brief  计数定时器初始化
 * @param  无
 * @return 无 
 */
void timer_init(void)
{
	RCC_APB1PeriphClockCmd(TIMER_CLK, ENABLE);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 10 - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIMER, &TIM_TimeBaseInitStructure);
	
	TIM_ClearFlag(TIMER, TIM_IT_Update);
	TIM_ITConfig(TIMER, TIM_IT_Update, ENABLE);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIMER_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIMER, ENABLE);
}

/**
 * @brief  获取开始后经过的时间
 * @param  无
 * @return 经过的时间
 */
uint32_t get_sys_ms(void)
{
	return sys_ms;
}


void TIM3_IRQHandler(void)
{
	if (TIM_GetITStatus(TIMER, TIM_IT_Update) == SET) {
		sys_ms++;
		TIM_ClearITPendingBit(TIMER, TIM_IT_Update);
	}
	
}

