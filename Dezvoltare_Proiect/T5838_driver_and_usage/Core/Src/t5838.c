#include "t5838.h"

HAL_StatusTypeDef T5838_Init(T5838_HandleTypeDef *dev, I2C_HandleTypeDef *hi2c, I2S_HandleTypeDef *hi2s,
                             GPIO_TypeDef *wake_port, uint16_t wake_pin,
                             GPIO_TypeDef *thsel_port, uint16_t thsel_pin) {
    dev->hi2c = hi2c;
    dev->hi2s = hi2s;
    dev->wake_port = wake_port;
    dev->wake_pin = wake_pin;
    dev->thsel_port = thsel_port;
    dev->thsel_pin = thsel_pin;
    dev->state = T5838_STATE_SLEEP_AAD;

    /* Setează pragul hardware pe LOW implicit */
    T5838_SetHardwareThresholdPin(dev, 0);

    return HAL_OK;
}

void T5838_SetHardwareThresholdPin(T5838_HandleTypeDef *dev, uint8_t high_level) {
    HAL_GPIO_WritePin(dev->thsel_port, dev->thsel_pin, high_level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

HAL_StatusTypeDef T5838_EnterAAD_D2Mode(T5838_HandleTypeDef *dev) {
    /* Oprirea generării de ceas I2S/PDM de la STM32 */
    HAL_I2S_DMAStop(dev->hi2s);

    /* Oprește ceasul I2S complet */
    __HAL_I2S_DISABLE(dev->hi2s);

    dev->state = T5838_STATE_SLEEP_AAD;
    return HAL_OK;
}

void T5838_OnWakeInterrupt(T5838_HandleTypeDef *dev) {
    if (dev->state == T5838_STATE_SLEEP_AAD) {
        dev->state = T5838_STATE_WOKEN_UP;
        /* La întreruperea WAKE, pornește semnalul PDM CLK și DMA-ul */
        T5838_StartPDMStream(dev);
    }
}

HAL_StatusTypeDef T5838_StartPDMStream(T5838_HandleTypeDef *dev) {
    HAL_StatusTypeDef status;

    /* Pornirea recepției DMA pe I2S2 - va activa PB10 (CLK) și PB15 (DATA) */
    status = HAL_I2S_Receive_DMA(dev->hi2s, (uint16_t*)dev->pdm_buffer, PDM_BUFFER_SIZE);

    if (status == HAL_OK) {
        dev->state = T5838_STATE_STREAMING;
    }
    return status;
}
