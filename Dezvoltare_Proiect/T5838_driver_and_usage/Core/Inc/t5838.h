#ifndef T5838_H
#define T5838_H

#include "stm32f4xx_hal.h"

#define T5838_THSEL_PORT        GPIOA
#define T5838_THSEL_PIN         GPIO_PIN_8
#define T5838_WAKE_PORT         GPIOB
#define T5838_WAKE_PIN          GPIO_PIN_1

#define T5838_PDM_BUF_SIZE      1024U       // cuvinte de 16 biti (jumatati de 512)
#define T5838_PCM_RING_SIZE     4096U       // putere a lui 2

#define T5838_DEV_ADDR_W        0xA6U       // 7'b1010011 + RW=0 (sec. 5.9)
#define T5838_SCK_WRITE_HZ      100000U     // scrieri / sleep: 50..200 kHz
#define T5838_SCK_CAPTURE_HZ    2400000U    // High Quality: 2.0..3.7 MHz

// 0 = registrii dupa harta (tab. 19/24), D2 enable = 0x02
// 1 = secventa verbatim din exemplul 5.31 (0x2B=0x24, enable=0x01)
#define T5838_DS_EXAMPLE_VERBATIM  0

#if T5838_DS_EXAMPLE_VERBATIM
  #define T5838_INIT_STEPS      12
#else
  #define T5838_INIT_STEPS      14
#endif

typedef struct {
    uint8_t reg;
    uint8_t data;
} T5838_RegisterConfig_t;

typedef struct {
    I2S_HandleTypeDef  *hi2s;
    UART_HandleTypeDef *huart;
    uint16_t pdm_buffer[T5838_PDM_BUF_SIZE];
    uint8_t  is_initialized;
    uint8_t  is_recording;
    uint32_t ring_overruns;
} T5838_HandleTypeDef;

// ACK (puls ~12 us pe WAKE) - setat din HAL_GPIO_EXTI_Callback
extern volatile uint8_t t5838_expect_ack;
extern volatile uint8_t t5838_ack_seen;

void              T5838_InitTiming(void);
uint32_t          T5838_SetClockFrequency(T5838_HandleTypeDef *hmic, uint32_t sck_hz); // 0 = eroare
void              T5838_ClockOn(T5838_HandleTypeDef *hmic);
void              T5838_ClockOff(T5838_HandleTypeDef *hmic);
void              T5838_WriteRegister(uint8_t reg, uint8_t data);
HAL_StatusTypeDef T5838_Init(T5838_HandleTypeDef *hmic, I2S_HandleTypeDef *hi2s, UART_HandleTypeDef *huart);
void              T5838_EnterAAD(T5838_HandleTypeDef *hmic);
HAL_StatusTypeDef T5838_StartCapture(T5838_HandleTypeDef *hmic);
void              T5838_StopRecording(T5838_HandleTypeDef *hmic);
void              T5838_ProcessAudioBlock(T5838_HandleTypeDef *hmic, uint16_t *pdm, uint16_t size);
void              T5838_FlushUart(T5838_HandleTypeDef *hmic);

#endif
