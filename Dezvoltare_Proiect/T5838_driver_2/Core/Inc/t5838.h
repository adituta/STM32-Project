#ifndef T5838_H
#define T5838_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ---- pini ---- */
#define T5838_THSEL_PORT        GPIOA
#define T5838_THSEL_PIN         GPIO_PIN_8
#define T5838_WAKE_PORT         GPIOB
#define T5838_WAKE_PIN          GPIO_PIN_1

/* ---- ceasuri (I2SCLK trebuie sa fie 38.4 MHz) ---- */
#define T5838_SCK_CFG_HZ        100000U     /* scrieri THSEL + sleep: 50..200 kHz */
#define T5838_SCK_CAPTURE_HZ    2400000U    /* High Quality: 2.0..3.7 MHz */

/* ---- decimare CIC: fs_pcm = 2.4 MHz / R ---- */
#define T5838_CIC_R             100U        /* 100 -> 24 kHz */
#define T5838_CIC_SHIFT         12          /* ~log2(R^4)-14: R=100 -> 12, R=50 -> 8 */
#define T5838_PCM_GAIN          1           /* amplificare software dupa decimare */

/* ---- buffere ---- */
#define T5838_PDM_BUF_WORDS     1024U       /* cuvinte de 16 biti, DMA circular */
#define T5838_PCM_RING_SIZE     4096U       /* esantioane int16, putere a lui 2 */
#define T5838_SKIP_HALF_BLOCKS  3U          /* ~10 ms aruncate dupa wake-up (6 ms in datasheet) */

/* ---- configurare AAD D2 (tab. 19/20/21/22/23) ---- */
#define T5838_AADD_ABS_TH       0x113U      /* 70 dB SPL; domeniu 0x00F..0x7BC */
#define T5838_AADD_FLOOR        0x016U      /* 45 dB SPL; domeniu 0x00F..0x7BC */
#define T5838_AADD_REL_TH       0x24U       /* +3 dB; domeniu 0x24..0xFF */
#define T5838_AADD_ABS_PULSE    0x000U      /* 12 biti, max 0xDAC */
#define T5838_AADD_REL_PULSE    0x000U      /* 12 biti, max 0x12C */
#define T5838_AADD_EN_VALUE     0x02U       /* tab. 19: 0x1=D1, 0x2=D2 (exemplul 5.31 scrie 0x01) */

/* 1 = adauga registrele nedocumentate din driverul Irnas (0x2C, 0x2D, bit 6 din 0x32) */
#define T5838_IRNAS_EXTRAS      0

typedef struct {
    I2S_HandleTypeDef  *hi2s;
    UART_HandleTypeDef *huart;
    uint16_t pdm_buffer[T5838_PDM_BUF_WORDS];
    uint8_t  initialized;
    uint8_t  ack_ok;
    uint8_t  recording;
    uint32_t block_overruns;    /* DMA a suprascris un bloc neprocesat */
    uint32_t ring_overruns;     /* UART prea lent */
} T5838_Handle_t;

HAL_StatusTypeDef T5838_Init(T5838_Handle_t *h, I2S_HandleTypeDef *hi2s, UART_HandleTypeDef *huart);
void              T5838_EnterAad(T5838_Handle_t *h);
HAL_StatusTypeDef T5838_StartCapture(T5838_Handle_t *h);
void              T5838_StopCapture(T5838_Handle_t *h);
void              T5838_Poll(T5838_Handle_t *h);       /* apeleaza continuu din main in timpul capturii */

/* evenimente */
void    T5838_WakeIrq(void);          /* din HAL_GPIO_EXTI_Callback */
uint8_t T5838_TakeWake(void);         /* 1 daca a fost un WAKE (si il consuma) */
void    T5838_DmaHalf(T5838_Handle_t *h);   /* din HAL_I2S_RxHalfCpltCallback */
void    T5838_DmaFull(T5838_Handle_t *h);   /* din HAL_I2S_RxCpltCallback */

/* nivel jos */
uint32_t T5838_SetSck(T5838_Handle_t *h, uint32_t sck_hz);   /* 0 = eroare */
void     T5838_ClockOn(T5838_Handle_t *h);
void     T5838_ClockOff(T5838_Handle_t *h);
void     T5838_WriteReg(uint8_t reg, uint8_t val);

#endif
