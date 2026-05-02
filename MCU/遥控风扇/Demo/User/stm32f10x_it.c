#include "tim.h"
#include "infrared_remote.h"


extern uint8_t key_code;
extern int32_t time_set;


void TIM1_CC_IRQHandler(void)
{
	uint32_t got_period = 0;
	uint32_t low_level_counters = 0;
	uint32_t high_level_counters = 0;
	
	if (TIM_GetITStatus(IR_TIM, TIM_IT_CC1) == SET) {
		got_period = TIM_GetCapture1(IR_TIM) + 1;
		low_level_counters = TIM_GetCapture2(IR_TIM) + 1;
		high_level_counters = got_period - low_level_counters;
		ir_decode_bit(low_level_counters, high_level_counters);
		
		TIM_SetCounter(IR_TIM, 0);
		TIM_ClearITPendingBit(IR_TIM, TIM_IT_CC1);
	}
}

void TIM3_IRQHandler(void)
{
		if (TIM_GetITStatus(TIMER, TIM_IT_Update) == SET) {
			if (time_set > 0) 
				time_set--;
			printf("time_set now remain %d S\r\n", time_set);
			
			TIM_ClearITPendingBit(TIMER, TIM_IT_Update);
		}
}
