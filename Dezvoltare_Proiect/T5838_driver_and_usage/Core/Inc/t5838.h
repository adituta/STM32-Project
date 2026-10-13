#ifndef T5838_H
#define T5838_H

#include "stm32f4xx_hal.h"

// constante pentru pinii folositi
#define T5838_THSEL_PORT	GPIOA
#define T5838_THSEL_PIN		GPIO_PIN_8

#define T5838_WAKE_PORT		GPIOB
#define T5838_WAKE_PIN		GPIO_PIN_1

#define T5838_INIT_STEPS	12
#define T5838_PDM_BUF_SIZE	1024


// structura in care pun perechile registru-valoare ale secventei de initializare (12 pasi)
typedef struct{
	uint8_t reg;
	uint8_t data;
}T5838_RegisterConfig_t;

typedef struct{
	I2S_HandleTypeDef *hi2s;
	UART_HandleTypeDef *huart;
	uint16_t pdm_buffer[T5838_PDM_BUF_SIZE];
	uint8_t is_initialized;
	uint8_t is_recording;
}T5838_HandleTypeDef;


void T5838_InitTiming(void);
HAL_StatusTypeDef T5838_Init(T5838_HandleTypeDef *hmic, I2S_HandleTypeDef *hi2s, UART_HandleTypeDef *huart);
uint8_t T5838_WriteRegister(uint8_t reg_addr, uint8_t reg_data);

void T5838_SetClockFrequency(T5838_HandleTypeDef* hmic, uint32_t freq_hz);
void T5838_OnWakeInterrupt(T5838_HandleTypeDef *hmic);
void T5838_ProcessAudioBlock(T5838_HandleTypeDef *hmic, uint16_t *pdm_data_ptr, uint16_t size);
void T5838_StopRecording(T5838_HandleTypeDef *hmic);

#endif



