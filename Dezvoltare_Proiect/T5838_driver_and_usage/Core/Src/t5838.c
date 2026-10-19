#include "t5838.h"
#include "main.h"
#include <stdio.h>

volatile uint8_t t5838_expect_ack = 0;
volatile uint8_t t5838_ack_seen   = 0;

static const T5838_RegisterConfig_t init_seq[T5838_INIT_STEPS] = {
    {0x5C,0x00},{0x3E,0x00},{0x6F,0x00},{0x3B,0x00},{0x4C,0x00},   // unlock (5.12)
#if T5838_DS_EXAMPLE_VERBATIM
    {0x31,0x13},{0x32,0x01},{0x36,0x04},{0x2B,0x24},{0x2E,0x00},{0x30,0x00},
    {0x29,0x01}
#else
    {0x31,0x13},{0x32,0x01},        // ABS_TH   = 0x113 (70 dB SPL)
    {0x2A,0x00},{0x2B,0x16},        // FLOOR    = 0x016 (45 dB SPL)
    {0x33,0x24},                    // REL_TH   = 0x24  (+3 dB)
    {0x2E,0x00},{0x2F,0x00},{0x30,0x00},   // pulse min = 0
    {0x29,0x02}                     // enable AAD D2 (ultima scriere)
#endif
};

#define THSEL_HIGH()  (T5838_THSEL_PORT->BSRR = T5838_THSEL_PIN)
#define THSEL_LOW()   (T5838_THSEL_PORT->BSRR = (uint32_t)T5838_THSEL_PIN << 16U)

static uint32_t tp_cycles, stop_cycles;          // in cicluri CPU, recalculate la fiecare SCK

static int16_t           pcm_ring[T5838_PCM_RING_SIZE];
static volatile uint32_t ring_head, ring_tail;
static uint32_t          tx_len;
static volatile uint8_t  skip_blocks;
static int32_t           hp_x, hp_y;

static inline void DelayCycles(uint32_t c){
    uint32_t s = DWT->CYCCNT;
    while ((DWT->CYCCNT - s) < c);
}

void T5838_InitTiming(void){
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ---------- ceas SCK (I2S master) ---------- */

// SCK = I2SCLK / (2*I2SDIV + ODD), MCLK dezactivat, cadru 16 biti. I2S ramane oprit.
uint32_t T5838_SetClockFrequency(T5838_HandleTypeDef *h, uint32_t sck_hz){
    uint32_t i2sclk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S);
    uint32_t total  = (i2sclk + sck_hz / 2U) / sck_hz;
    uint32_t div    = total >> 1;
    uint32_t odd    = total & 1U;
    if (div < 2U || div > 255U) return 0;

    __HAL_I2S_DISABLE(h->hi2s);
    h->hi2s->Instance->I2SPR = (div << SPI_I2SPR_I2SDIV_Pos) | (odd << SPI_I2SPR_ODD_Pos);

    uint32_t real       = i2sclk / total;
    uint32_t clk_cycles = SystemCoreClock / real;     // cicluri CPU per perioada CLK
    tp_cycles   = 10U  * clk_cycles;                  // Tp = 10 perioade CLK (tipic)
    stop_cycles = 140U * clk_cycles;                  // STOP > 128 perioade CLK
    return real;
}

void T5838_ClockOn(T5838_HandleTypeDef *h){
    __HAL_I2S_ENABLE(h->hi2s);
}

void T5838_ClockOff(T5838_HandleTypeDef *h){
    __HAL_I2S_DISABLE(h->hi2s);
    volatile uint32_t t = h->hi2s->Instance->DR;      // curata un eventual overrun ramas
    t = h->hi2s->Instance->SR;
    (void)t;
}

/* ---------- protocol one-wire pe THSEL (5.9 - 5.11) ---------- */

static void Symbol(uint32_t high_cycles){             // simbol HIGH + Space (LOW, 1 x Tp)
    THSEL_HIGH();
    DelayCycles(high_cycles);
    THSEL_LOW();
    DelayCycles(tp_cycles);
}

static void SendByte(uint8_t b){
    for (int8_t i = 7; i >= 0; i--)
        Symbol(((b >> i) & 1U) ? (3U * tp_cycles) : tp_cycles);   // One = 3Tp, Zero = 1Tp
}

void T5838_WriteRegister(uint8_t reg, uint8_t data){
    Symbol(tp_cycles);                // START/PILOT = Tp
    SendByte(T5838_DEV_ADDR_W);
    SendByte(reg);
    SendByte(data);
    THSEL_HIGH();                     // STOP: HIGH > 128 perioade CLK
    DelayCycles(stop_cycles);
    THSEL_LOW();
    DelayCycles(2U * tp_cycles);
}

/* ---------- initializare si moduri ---------- */

HAL_StatusTypeDef T5838_Init(T5838_HandleTypeDef *h, I2S_HandleTypeDef *hi2s, UART_HandleTypeDef *huart){
    h->hi2s = hi2s;  h->huart = huart;
    h->is_initialized = 0;  h->is_recording = 0;  h->ring_overruns = 0;

    T5838_InitTiming();

    uint32_t sck = T5838_SetClockFrequency(h, T5838_SCK_WRITE_HZ);
    printf("\r\n[T5838] I2SCLK=%lu Hz, SCK scriere=%lu Hz\r\n",
           (unsigned long)HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S), (unsigned long)sck);
    if (sck == 0) { printf("[T5838] Divizor I2S invalid\r\n"); return HAL_ERROR; }
    if (huart->hdmatx == NULL) printf("[T5838] ATENTIE: USART2 TX nu are DMA (adauga-l in .ioc)\r\n");

    T5838_ClockOn(h);                 // scrierile cer CLK activ
    HAL_Delay(10);

    printf("[T5838] Pornire secventa de configurare...\r\n");
    for (uint8_t i = 0; i < T5838_INIT_STEPS; i++){
        uint8_t last = (i == T5838_INIT_STEPS - 1U);
        if (last){ t5838_ack_seen = 0; t5838_expect_ack = 1; }

        T5838_WriteRegister(init_seq[i].reg, init_seq[i].data);

        if (last){                    // ACK = puls ~12 us pe WAKE, doar la enable
            uint32_t t0 = HAL_GetTick();
            while (!t5838_ack_seen && (HAL_GetTick() - t0) < 5U);
            t5838_expect_ack = 0;
            if (!t5838_ack_seen){
                T5838_ClockOff(h);
                printf("[T5838] Fara ACK pe WAKE dupa enable\r\n");
                return HAL_ERROR;
            }
        }
    }
    h->is_initialized = 1;
    T5838_EnterAAD(h);
    printf("[T5838] ACK primit, AAD D2 activ (CLK oprit)\r\n");
    return HAL_OK;
}

// 5.31 pasul 5: CLK 50..200 kHz minim 2 ms, apoi CLK OFF
void T5838_EnterAAD(T5838_HandleTypeDef *h){
    T5838_ClockOff(h);
    T5838_SetClockFrequency(h, T5838_SCK_WRITE_HZ);
    T5838_ClockOn(h);
    HAL_Delay(3);
    T5838_ClockOff(h);
}

// iesire din AAD direct in High Quality; wake-up ~6 ms, primele ~10 ms se arunca
HAL_StatusTypeDef T5838_StartCapture(T5838_HandleTypeDef *h){
    if (!h->is_initialized || h->is_recording) return HAL_BUSY;

    T5838_ClockOff(h);
    if (T5838_SetClockFrequency(h, T5838_SCK_CAPTURE_HZ) == 0) return HAL_ERROR;

    ring_head = ring_tail = 0;  tx_len = 0;  hp_x = hp_y = 0;
    skip_blocks = 3;                          // 3 x ~3.4 ms
    h->is_recording = 1;

    HAL_StatusTypeDef st = HAL_I2S_Receive_DMA(h->hi2s, h->pdm_buffer, T5838_PDM_BUF_SIZE);
    if (st != HAL_OK) h->is_recording = 0;
    return st;
}

void T5838_StopRecording(T5838_HandleTypeDef *h){
    if (!h->is_recording) return;
    HAL_I2S_DMAStop(h->hi2s);
    h->is_recording = 0;
    T5838_EnterAAD(h);                        // sleep intermediar, apoi CLK OFF
}

/* ---------- PDM -> PCM (rulat din ISR-ul DMA) ---------- */

void T5838_ProcessAudioBlock(T5838_HandleTypeDef *h, uint16_t *p, uint16_t size){
    (void)h;
    if (skip_blocks){ skip_blocks--; return; }

    for (uint16_t i = 0; (uint16_t)(i + 8U) <= size; i += 8U){   // 128 biti PDM -> 1 esantion
        int32_t d = 0;
        for (uint8_t j = 0; j < 8U; j++) d += __builtin_popcount(p[i + j]);

        int32_t x = (d - 64) * 128;
        int32_t y = x - hp_x + hp_y - (hp_y >> 8);   // DC block: pol ~0.996, colt ~12 Hz
        hp_x = x;  hp_y = y;

        if (y >  32767) y =  32767;
        if (y < -32768) y = -32768;

        pcm_ring[ring_head & (T5838_PCM_RING_SIZE - 1U)] = (int16_t)y;
        ring_head++;
    }
}

/* ---------- PCM -> UART (apelata din bucla main) ---------- */

void T5838_FlushUart(T5838_HandleTypeDef *h){
    if (h->huart->hdmatx == NULL) return;
    if (h->huart->gState != HAL_UART_STATE_READY) return;

    ring_tail += tx_len;  tx_len = 0;          // transferul anterior s-a terminat

    uint32_t avail = ring_head - ring_tail;
    if (avail > T5838_PCM_RING_SIZE){          // producatorul a depasit consumatorul
        h->ring_overruns++;
        ring_tail = ring_head;
        return;
    }
    uint32_t idx = ring_tail & (T5838_PCM_RING_SIZE - 1U);
    uint32_t n   = avail;
    if (idx + n > T5838_PCM_RING_SIZE) n = T5838_PCM_RING_SIZE - idx;
    if (n == 0) return;

    if (HAL_UART_Transmit_DMA(h->huart, (uint8_t*)&pcm_ring[idx], (uint16_t)(n * 2U)) == HAL_OK)
        tx_len = n;
}
