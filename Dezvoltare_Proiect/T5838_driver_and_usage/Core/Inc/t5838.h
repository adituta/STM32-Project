#ifndef INC_T5838_H_
#define INC_T5838_H_

#include "main.h"

/* Adresa I2C implicită a T5838 */
#define T5838_I2C_ADDR         (0x38 << 1)

/* Dimensiunea bufferului PDM (eșantioane de 16 biți) */
#define PDM_BUFFER_SIZE        1024

typedef enum {
    T5838_STATE_SLEEP_AAD = 0,
    T5838_STATE_WOKEN_UP,
    T5838_STATE_STREAMING
} T5838_State_t;

typedef struct {
    I2C_HandleTypeDef *hi2c;
    I2S_HandleTypeDef *hi2s;
    GPIO_TypeDef      *wake_port;
    uint16_t           wake_pin;
    GPIO_TypeDef      *thsel_port;
    uint16_t           thsel_pin;
    volatile T5838_State_t state;
    uint16_t           pdm_buffer[PDM_BUFFER_SIZE];
} T5838_HandleTypeDef;

/* API Driver */
HAL_StatusTypeDef T5838_Init(T5838_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, I2S_HandleTypeDef *hi2s,
                             GPIO_TypeDef *wake_port, uint16_t wake_pin,
                             GPIO_TypeDef *thsel_port, uint16_t thsel_pin);

void T5838_SetHardwareThresholdPin(T5838_HandleTypeDef *dev, uint8_t high_level);
HAL_StatusTypeDef T5838_EnterAAD_D2Mode(T5838_HandleTypeDef *dev);
void T5838_OnWakeInterrupt(T5838_HandleTypeDef *dev);
HAL_StatusTypeDef T5838_StartPDMStream(T5838_HandleTypeDef *dev);

#endif /* INC_T5838_H_ */
