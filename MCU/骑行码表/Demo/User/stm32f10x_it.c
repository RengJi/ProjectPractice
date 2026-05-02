#include "tim.h"  
#include "GPS.h"
#include "usart.h"


void USART1_IRQHandler(void)
{
	uint8_t rx_data = 0;
	if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET) {
		rx_data = USART_ReceiveData(USART3);
		usart1_send_byte(rx_data);    //收到什么就发出去什么
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

void USART3_IRQHandler(void)
{
	uint8_t rx_data = 0;
	if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET) {
		rx_data = USART_ReceiveData(USART3);
		gps_data_received(rx_data);
		USART_ClearITPendingBit(USART3, USART_IT_RXNE);
	}
}
