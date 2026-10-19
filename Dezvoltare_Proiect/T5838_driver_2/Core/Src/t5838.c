#include "t5838.h"
#include <stdio.h>

#define DEV_ADDR_W   0xA6U      /* 7'b1010011 + RW=0 */
#define TP_CLKS      10U        /* pilot = Tp = 10 perioade CLK */
#define STOP_CLKS    130U       /* > 128 perioade CLK */
#define GUARD_CLKS   60U        /* CLK activ inainte de START / dupa STOP (ca la Irnas, >50) */

typedef struct { uint8_t reg; uint8_t val; } reg_t;

static volatile uint8_t ack_window, ack_seen, wake_flag;
static uint32_t cyc_clk;                        /* cicluri CPU / perioada SCK */

#define THSEL_HIGH()  (T5838_THSEL_PORT->BSRR = T5838_THSEL_PIN)
#define THSEL_LOW()   (T5838_THSEL_PORT->BSRR = (uint32_t)T5838_THSEL_PIN << 16U)

static inline void DelayCycles(uint32_t c){
    uint32_t s = DWT->CYCCNT;
    while ((DWT->CYCCNT - s) < c) {}
}
static inline void DelayClk(uint32_t n){ DelayCycles(n * cyc_clk); }

/* ================= ceas SCK (I2S master) ================= */

/* SCK = I2SCLK / (2*I2SDIV + ODD), MCLK dezactivat, cadru 16 biti */
uint32_t T5838_SetSck(T5838_Handle_t *h, uint32_t sck_hz){
    uint32_t i2sclk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S);
    if (i2sclk == 0U || sck_hz == 0U) return 0;

    uint32_t total = (i2sclk + sck_hz / 2U) / sck_hz;
    uint32_t div = total >> 1, odd = total & 1U;
    if (div < 2U || div > 255U) return 0;

    __HAL_I2S_DISABLE(h->hi2s);
    uint32_t pr = h->hi2s->Instance->I2SPR;
    pr &= ~(SPI_I2SPR_I2SDIV | SPI_I2SPR_ODD);
    pr |= (div << SPI_I2SPR_I2SDIV_Pos) | (odd << SPI_I2SPR_ODD_Pos);
    h->hi2s->Instance->I2SPR = pr;

    uint32_t real = i2sclk / total;
    cyc_clk = SystemCoreClock / real;
    return real;
}

void T5838_ClockOn(T5838_Handle_t *h){ __HAL_I2S_ENABLE(h->hi2s); }

void T5838_ClockOff(T5838_Handle_t *h){
    __HAL_I2S_DISABLE(h->hi2s);
    volatile uint32_t t = h->hi2s->Instance->DR;    /* curata un eventual overrun */
    t = h->hi2s->Instance->SR;
    (void)t;
}

/* ================= protocol one-wire pe THSEL (sec. 5.9-5.11) ================= */

static void Sym(uint32_t high_clks){        /* simbol HIGH + Space (LOW, 1 x Tp) */
    THSEL_HIGH();
    DelayClk(high_clks);
    THSEL_LOW();
    DelayClk(TP_CLKS);
}

static void SendByte(uint8_t b){
    for (int8_t i = 7; i >= 0; i--)
        Sym(((b >> i) & 1U) ? (3U * TP_CLKS) : TP_CLKS);   /* One = 3Tp, Zero = 1Tp */
}

void T5838_WriteReg(uint8_t reg, uint8_t val){
    THSEL_LOW();
    DelayClk(GUARD_CLKS);
    Sym(TP_CLKS);                           /* START / PILOT */
    SendByte(DEV_ADDR_W);
    SendByte(reg);
    SendByte(val);
    THSEL_HIGH();                           /* STOP */
    DelayClk(STOP_CLKS);
    THSEL_LOW();
    DelayClk(GUARD_CLKS);
}

static void WriteSeq(const reg_t *s, uint32_t n){
    for (uint32_t i = 0; i < n; i++) T5838_WriteReg(s[i].reg, s[i].val);
}

/* ================= evenimente WAKE ================= */

void T5838_WakeIrq(void){
    if (ack_window) ack_seen = 1;           /* pulsul ~12 us de la enable */
    else            wake_flag = 1;          /* eveniment AAD */
}
uint8_t T5838_TakeWake(void){
    if (!wake_flag) return 0;
    wake_flag = 0;
    return 1;
}

/* ================= initializare AAD D2 ================= */

HAL_StatusTypeDef T5838_Init(T5838_Handle_t *h, I2S_HandleTypeDef *hi2s, UART_HandleTypeDef *huart){
    h->hi2s = hi2s;  h->huart = huart;
    h->initialized = 0;  h->ack_ok = 0;  h->recording = 0;
    h->block_overruns = 0;  h->ring_overruns = 0;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;     /* contor DWT */
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    THSEL_LOW();

    uint32_t sck = T5838_SetSck(h, T5838_SCK_CFG_HZ);
    printf("[T5838] I2SCLK=%lu Hz, SCK cfg=%lu Hz\r\n",
           (unsigned long)HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S), (unsigned long)sck);
    if (sck == 0U) { printf("[T5838] divizor I2S invalid\r\n"); return HAL_ERROR; }

    T5838_ClockOn(h);                       /* scrierile cer CLK activ */
    HAL_Delay(10);

    static const reg_t unlock[] = {         /* sec. 5.12 */
        {0x5C,0x00},{0x3E,0x00},{0x6F,0x00},{0x3B,0x00},{0x4C,0x00}
    };
    const reg_t cfg[] = {                   /* harta de registre, tab. 24 */
        {0x29, 0x00},                                            /* AAD oprit cat configurez */
        {0x2A, (T5838_AADD_FLOOR >> 8) & 0x1F},
        {0x2B,  T5838_AADD_FLOOR & 0xFF},
        {0x2E,  T5838_AADD_REL_PULSE & 0xFF},
        {0x2F, (uint8_t)((((T5838_AADD_ABS_PULSE >> 8) & 0x0F) << 4) |
                          ((T5838_AADD_REL_PULSE >> 8) & 0x0F))},
        {0x30,  T5838_AADD_ABS_PULSE & 0xFF},
        {0x31,  T5838_AADD_ABS_TH & 0xFF},
#if T5838_IRNAS_EXTRAS
        {0x32, (uint8_t)(((T5838_AADD_ABS_TH >> 8) & 0x1F) | 0x40)},
        {0x2C, 0x32}, {0x2D, 0xC0},
#else
        {0x32, (T5838_AADD_ABS_TH >> 8) & 0x1F},
#endif
        {0x33,  T5838_AADD_REL_TH},
    };
    WriteSeq(unlock, sizeof(unlock) / sizeof(unlock[0]));
    WriteSeq(cfg,    sizeof(cfg)    / sizeof(cfg[0]));

    ack_seen = 0;  ack_window = 1;
    T5838_WriteReg(0x29, T5838_AADD_EN_VALUE);             /* enable -> ACK pe WAKE */
    uint32_t t0 = HAL_GetTick();
    while (!ack_seen && (HAL_GetTick() - t0) < 5U) {}
    ack_window = 0;

    h->ack_ok = ack_seen;
    h->initialized = 1;
    printf(h->ack_ok ? "[T5838] ACK primit pe WAKE\r\n"
                     : "[T5838] FARA ACK pe WAKE dupa enable\r\n");

    T5838_EnterAad(h);
    wake_flag = 0;
    return h->ack_ok ? HAL_OK : HAL_ERROR;
}

/* sec. 5.3/5.31: CLK 50..200 kHz minim 2 ms, apoi CLK OFF */
void T5838_EnterAad(T5838_Handle_t *h){
    T5838_ClockOff(h);
    T5838_SetSck(h, T5838_SCK_CFG_HZ);
    T5838_ClockOn(h);
    HAL_Delay(3);
    T5838_ClockOff(h);
}

/* ================= decimare PDM -> PCM (CIC ordin 4 + DC block) ================= */

static uint32_t i1, i2, i3, i4, c1, c2, c3, c4, cic_cnt;
static int32_t  dc_x, dc_y;
static int16_t  pcm_ring[T5838_PCM_RING_SIZE];
static uint32_t ring_head, ring_tail, tx_len;
static volatile uint8_t half_ready, full_ready;
static uint8_t  skip_blocks;

static void CicReset(void){
    i1 = i2 = i3 = i4 = c1 = c2 = c3 = c4 = cic_cnt = 0;
    dc_x = dc_y = 0;
    ring_head = ring_tail = tx_len = 0;
}

static void ProcessBlock(const uint16_t *p, uint16_t words){
    for (uint16_t k = 0; k < words; k++) {
        uint32_t w = p[k];
        for (int8_t b = 15; b >= 0; b--) {                 /* MSB primul = ordine in timp */
            uint32_t x = ((w >> b) & 1U) ? 1U : 0xFFFFFFFFU;   /* +1 / -1 */
            i1 += x; i2 += i1; i3 += i2; i4 += i3;
            if (++cic_cnt == T5838_CIC_R) {
                cic_cnt = 0;
                uint32_t t = i4, d;
                d = t - c1; c1 = t; t = d;
                d = t - c2; c2 = t; t = d;
                d = t - c3; c3 = t; t = d;
                d = t - c4; c4 = t; t = d;

                int32_t y  = ((int32_t)t >> T5838_CIC_SHIFT) * T5838_PCM_GAIN;
                int32_t yo = y - dc_x + dc_y - (dc_y >> 8);   /* high-pass ~ 15 Hz la 24 kHz */
                dc_x = y;  dc_y = yo;
                if (yo >  32767) yo =  32767;
                if (yo < -32768) yo = -32768;
                pcm_ring[ring_head & (T5838_PCM_RING_SIZE - 1U)] = (int16_t)yo;
                ring_head++;
            }
        }
    }
}

static void FlushUart(T5838_Handle_t *h){
    if (h->huart->hdmatx == NULL) return;
    if (h->huart->gState != HAL_UART_STATE_READY) return;

    ring_tail += tx_len;  tx_len = 0;                      /* transferul anterior s-a terminat */
    uint32_t avail = ring_head - ring_tail;
    if (avail > T5838_PCM_RING_SIZE) {                     /* UART prea lent */
        h->ring_overruns++;
        ring_tail = ring_head;
        return;
    }
    uint32_t idx = ring_tail & (T5838_PCM_RING_SIZE - 1U);
    uint32_t n = avail;
    if (idx + n > T5838_PCM_RING_SIZE) n = T5838_PCM_RING_SIZE - idx;
    if (n == 0U) return;
    if (HAL_UART_Transmit_DMA(h->huart, (uint8_t *)&pcm_ring[idx], (uint16_t)(n * 2U)) == HAL_OK)
        tx_len = n;
}

/* ================= captura ================= */

void T5838_DmaHalf(T5838_Handle_t *h){ if (half_ready) h->block_overruns++; half_ready = 1; }
void T5838_DmaFull(T5838_Handle_t *h){ if (full_ready) h->block_overruns++; full_ready = 1; }

HAL_StatusTypeDef T5838_StartCapture(T5838_Handle_t *h){
    if (!h->initialized || h->recording) return HAL_BUSY;
    if (h->huart->hdmatx == NULL) { printf("[T5838] USART2 TX fara DMA\r\n"); return HAL_ERROR; }

    T5838_ClockOff(h);
    if (T5838_SetSck(h, T5838_SCK_CAPTURE_HZ) == 0U) return HAL_ERROR;

    CicReset();
    half_ready = full_ready = 0;
    skip_blocks = T5838_SKIP_HALF_BLOCKS;
    h->block_overruns = 0;  h->ring_overruns = 0;
    h->recording = 1;

    HAL_StatusTypeDef st = HAL_I2S_Receive_DMA(h->hi2s, h->pdm_buffer, T5838_PDM_BUF_WORDS);
    if (st != HAL_OK) h->recording = 0;
    return st;
}

void T5838_Poll(T5838_Handle_t *h){
    if (!h->recording) return;
    const uint16_t half = T5838_PDM_BUF_WORDS / 2U;

    if (half_ready) {
        half_ready = 0;
        if (skip_blocks) skip_blocks--; else ProcessBlock(&h->pdm_buffer[0], half);
    }
    if (full_ready) {
        full_ready = 0;
        if (skip_blocks) skip_blocks--; else ProcessBlock(&h->pdm_buffer[half], half);
    }
    FlushUart(h);
}

void T5838_StopCapture(T5838_Handle_t *h){
    if (!h->recording) return;
    HAL_I2S_DMAStop(h->hi2s);
    h->recording = 0;

    uint32_t t0 = HAL_GetTick();                           /* goleste ce a ramas in ring */
    while ((HAL_GetTick() - t0) < 300U) {
        FlushUart(h);
        if (h->huart->gState == HAL_UART_STATE_READY && ring_head == ring_tail && tx_len == 0U) break;
    }
    T5838_EnterAad(h);                                     /* sleep intermediar -> CLK OFF */
    HAL_Delay(10);
    wake_flag = 0;
    if (HAL_GPIO_ReadPin(T5838_WAKE_PORT, T5838_WAKE_PIN) == GPIO_PIN_SET)
        wake_flag = 1;                                     /* zgomotul continua: WAKE deja sus */
}
