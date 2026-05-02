#include "tim.h"

/**
 * @brief  红外接收需要的中断定时器初始化
 * @param  无
 * @return 无
 */
void ir_tim_init(void)
{
	RCC_APB1PeriphClockCmd(IR_TIM_CLK, ENABLE);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(IR_TIM, &TIM_TimeBaseInitStructure);
	
	TIM_ClearFlag(IR_TIM, TIM_IT_Update);
	TIM_ITConfig(IR_TIM, TIM_IT_Update, ENABLE);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = IR_IT_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(IR_TIM, ENABLE);
}

