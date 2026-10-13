#include "t5838.h"
#include<stdio.h>

// secventa de configurare I2C pentru THSEL in modul AAD D2
static const T5838_RegisterConfig_t t5838_init_sequence[T5838_INIT_STEPS]={
		{0x5C, 0x00}, {0x3E, 0x00}, {0x6F, 0x00}, {0x3B, 0x00},
		{0x4C, 0x00}, {0x31, 0x13}, {0x32, 0x01}, {0x36, 0x04},
		{0x2B, 0x24}, {0x2E, 0x00}, {0x30, 0x00}, {0x29, 0x01}
};


// functii private (helper funcitons)
static inline void DelayCycles(uint32_t cycles){
	uint32_t start = DWT->CYCCNT;
	while((DWT->CYCCNT-start)<cycles);
}

// a se nota ca toate define-urile referitoare la durata semnalelor de mai jos sunt facute
// in functie de frecventa maxima a MCU-ului pentru cazul acesta experimental, adica 84MHz
#define CYCLES_TC		420U
#define CYCLES_BIT0		(1U*CYCLES_TC)
#define CYCLES_BIT1		(3U*CYCLES_TC)
#define CYCLES_SPACE	(1U*CYCLES_TC)
#define CYCLES_STOP		(128U* 42U)

static void T5838_SendBit(uint8_t bit){
	HAL_GPIO_WritePin(T5838_THSEL_PORT, T5838_THSEL_PIN, GPIO_PIN_SET);
	DelayCycles(bit ? CYCLES_BIT1 : CYCLES_BIT0);

	HAL_GPIO_WritePin(T5838_THSEL_PORT, T5838_THSEL_PIN, GPIO_PIN_RESET);
	DelayCycles(CYCLES_SPACE);
}

static void T5838_SendByte(uint8_t byte){
	for(int8_t i = 7;i>=0;i--){
		T5838_SendBit((byte>>i)&0x01);
	}
}

// functii publice
uint8_t T5838_WriteRegister(uint8_t reg_addr, uint8_t reg_data){
	__disable_irq();
	T5838_SendByte(reg_addr);
	T5838_SendByte(reg_data);

	//secventa STOP
	HAL_GPIO_WritePin(T5838_THSEL_PORT, T5838_THSEL_PIN, GPIO_PIN_RESET);
	DelayCycles(CYCLES_STOP);
	__enable_irq();

	return 1;		// ack validat
}

HAL_StatusTypeDef T5838_Init(T5838_HandleTypeDef *hmic, I2S_HandleTypeDef* hi2s, UART_HandleTypeDef* huart){
	hmic->hi2s = hi2s;
	hmic->huart = huart;
	hmic->is_initialized = 0;
	hmic->is_recording = 0;

	T5838_InitTiming();

	printf("\r\n[T5838] Pornire secventa de configurare...\r\n");

	for(uint8_t i = 0;i<T5838_INIT_STEPS;i++){
		uint8_t ok = T5838_WriteRegister(t5838_init_sequence[i].reg, t5838_init_sequence[i].data);
		if(!ok){
			printf("[T5838] EROARE la pasul %d (Reg 0x%02X)\r\n", i, t5838_init_sequence[i].reg);
			return HAL_ERROR;
		}
	}

	hmic->is_initialized = 1;
	printf("[T5838] Configurat cu succes in modul AAD D2!\r\n");

	return HAL_OK;
}

void T5838_OnWakeInterrupt(T5838_HandleTypeDef *hmic){
	if(hmic->is_initialized && !hmic->is_recording){
		hmic->is_recording = 1;

		HAL_I2S_Receive_DMA(hmic->hi2s, hmic->pdm_buffer, T5838_PDM_BUF_SIZE);
	}
}

void T5838_SetClockFrequency(T5838_HandleTypeDef *hmic, uint32_t freq_hz){
	// oprire I2S pentru reconfigurare registru
	__HAL_I2S_DISABLE(hmic->hi2s);

	if(freq_hz==100000){
		SPI2->I2SPR = (SPI2->I2SPR & ~SPI_I2SPR_I2SDIV) | 125;
	}else{
		SPI2->I2SPR = (SPI2->I2SPR & ~SPI_I2SPR_I2SDIV) | 6;
	}

	__HAL_I2S_ENABLE(hmic->hi2s);
}



// procesare buffer PDM si trimitere date prin uart
//void T5838_ProcessAudioBlock(T5838_HandleTypeDef *hmic, uint16_t *pdm_data_ptr, uint16_t size){
/*	// buffer temporar PCM extras prin numarare bit-density (Simple Decimation Filter)
	int16_t pcm_samples[64];
	uint16_t pcm_index = 0;

	for(uint16_t i = 0;i<size && pcm_index <64; i+=8){
		int16_t density = 0;
		for(uint8_t j = 0;j<8;j++){
			// numara cati biti de 1 am in esantionul de 16 biti
			density+=__builtin_popcount(pdm_data_ptr[i+j]);
		}

		//scalare in interval de semnal audio 16-bit signed (-32768 la 32767)
		pcm_samples[pcm_index++] = (density-64)*512;
	}

	//Transmitere pachet PCM brut prin uart catre terminalul din PC
	HAL_UART_Transmit(hmic->huart, (uint8_t*)pcm_samples, pcm_index *sizeof(int16_t), 10);

	*/


	/* Bloc 2 de procesare (incercarea 2)
	printf("P");
	int16_t pcm_samples[64];
	    uint16_t pcm_index = 0;

	    // Variabile statice pentru filtrul DC block (High-Pass)
	    static int32_t x_prev = 0;
	    static int32_t y_prev = 0;

	    for (uint16_t i = 0; i < size && pcm_index < 64; i += 8) {
	        int32_t density = 0;
	        for (uint8_t j = 0; j < 8; j++) {
	            // Numărăm biții de 1 din datele PDM
	            density += __builtin_popcount(pdm_data_ptr[i + j]);
	        }

	        // 1. Scalare brută și centrare în jurul originii (density e între 0 și 128)
	        int32_t raw_pcm = (density - 64) * 64; // Redus de la 512 la 64 pentru a evita clipping-ul

	        // 2. Filtru simplu DC Block (High-Pass Filter)
	        // y[n] = x[n] - x[n-1] + 0.95 * y[n-1]
	        int32_t filtered = raw_pcm - x_prev + (y_prev * 950 / 1000);

	        x_prev = raw_pcm;
	        y_prev = filtered;

	        // 3. Limitează semnalul (Saturate) la int16_t (-32768 la 32767)
	        if (filtered > 32767) filtered = 32767;
	        if (filtered < -32768) filtered = -32768;

	        pcm_samples[pcm_index++] = (int16_t)filtered;
	    }

	    // Transmitere prin UART
	    HAL_UART_Transmit(hmic->huart, (uint8_t*)pcm_samples, pcm_index * sizeof(int16_t), 10);
*/


	// Buffer temporar pentru eșantioanele PCM calculate din blocul curent PDM
	    // Dacă size = 128 cuvinte (16-bit), obținem 16 eșantioane PCM (Decimare factor 64)
/*
	    int16_t pcm_samples[16];
	    uint16_t pcm_index = 0;

	    // Variabile statice pentru filtrul DC-Block (High-Pass IIR)
	    static int32_t x_prev = 0;
	    static int32_t y_prev = 0;

	    for (uint16_t i = 0; i < size && pcm_index < 16; i += 8) {
	        int32_t density = 0;

	        // Accumulate biți de 1 cu instrucțiunea hardware POPCOUNT
	        for (uint8_t j = 0; j < 8; j++) {
	            density += __builtin_popcount(pdm_data_ptr[i + j]);
	        }

	        // 1. Centrare pe 0 (density are valori între 0 și 128, reprezentând media PDM)
	        // Gain moderat (x128) pentru dynamic range optim
	        int32_t raw_pcm = (density - 64) * 128;

	        // 2. Filtru High-Pass IIR (DC Block Filter)
	        // Elimină offset-ul DC continuu care provoca saturația anterioară
	        // y[n] = x[n] - x[n-1] + 0.95 * y[n-1]
	        int32_t filtered = raw_pcm - x_prev + ((y_prev * 950) >> 10); // Schimbat la shiftare pentru viteză

	        x_prev = raw_pcm;
	        y_prev = filtered;

	        // 3. Dynamic Range Control / Hard Limiter (evită clipping-ul la int16)
	        if (filtered > 32767)   filtered = 32767;
	        if (filtered < -32768)  filtered = -32768;

	        pcm_samples[pcm_index++] = (int16_t)filtered;
	    }

	    // Transmitere rapidă directă a datelor brute PCM pe UART
	    HAL_UART_Transmit(hmic->huart, (uint8_t*)pcm_samples, pcm_index * sizeof(int16_t), 10);

}

*/


void T5838_ProcessAudioBlock(T5838_HandleTypeDef *hmic, uint16_t *pdm_data_ptr, uint16_t size) {
    int16_t pcm_samples[16];
    uint16_t pcm_index = 0;

    static int32_t x_prev = 0;
    static int32_t y_prev = 0;

    for (uint16_t i = 0; i < size && pcm_index < 16; i += 8) {
        int32_t density = 0;
        for (uint8_t j = 0; j < 8; j++) {
            density += __builtin_popcount(pdm_data_ptr[i + j]);
        }

        // Centrare și scalare moderată
        int32_t raw_pcm = (density - 64) * 128;

        // High-Pass Filter (DC Block)
        int32_t filtered = raw_pcm - x_prev + ((y_prev * 950) >> 10);

        x_prev = raw_pcm;
        y_prev = filtered;

        // Limiter / Saturation guard
        if (filtered > 32767)   filtered = 32767;
        if (filtered < -32768)  filtered = -32768;

        pcm_samples[pcm_index++] = (int16_t)filtered;
    }

    // Transmitere rapidă a pachetului PCM procesat
    HAL_UART_Transmit(hmic->huart, (uint8_t*)pcm_samples, pcm_index * sizeof(int16_t), 10);
}

// functie de oprire inregistrare PDM
void T5838_StopRecording(T5838_HandleTypeDef *hmic){
	if(hmic->is_recording){
		HAL_I2S_DMAStop(hmic->hi2s);
		hmic->is_recording = 0;
		printf("[T5838] Inregistrare oprita. Trecere in Low-Power.\r\n");
	}
}





void T5838_InitTiming(void) {
    // Activează DWT (Data Watchpoint and Trace) pentru contorizarea exactă a ciclurilor de ceas
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}











