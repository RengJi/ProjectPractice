#include "tim.h"
#include "sound.h"
#include "body_detect.h"
#include "infrared_remote.h"


extern uint8_t key_code;

int is_sound_loud = 0;
int is_body_pass = 0;

/* 声音传感器触发中断 */
void EXTI1_IRQHandler(void)
{
	if (EXTI_GetITStatus(SOUND_EXTI_LINE) == SET) {
		if (GPIO_ReadInputDataBit(SOUND_PORT, SOUND_PIN) == Bit_SET) {
			is_sound_loud = 1;
		}
		EXTI_ClearITPendingBit(SOUND_EXTI_LINE);
	}
}

/* 人体传感器触发 */
void EXTI4_IRQHandler(void)
{
	if (EXTI_GetITStatus(BODY_DETECT_EXTI_LINE) == SET) {
		if (GPIO_ReadInputDataBit(BODY_DETECT_PORT, BODY_DETECT_PIN) == Bit_SET) {
			is_body_pass = 1;
		}
		EXTI_ClearITPendingBit(BODY_DETECT_EXTI_LINE);
	}
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(IR_TIM, TIM_IT_Update) == SET) {
		ir_measure_bit_level_every_100us();
		TIM_ClearITPendingBit(IR_TIM, TIM_IT_Update);
	}
}
