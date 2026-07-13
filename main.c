/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include<stdio.h>
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

	uint8_t data[] = "Hello world\r\n";
	uint32_t numar = 123;
	uint8_t sir_numar[5];

	uint8_t TxData[10240];
	int isSent = 1;

	int countloop = 0;
	int countinterrupt = 0;

	int indx=49;		// char '1'

	//Callback-uri
	// se declanseaza cand DMA a trimis prima jumatate din date. In acest moment
	//se rescrie prima jumatate a bufferului cu acelasi octet (indx) apoi incrementeaza indx
	void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef *huart){
		for(uint32_t i = 0;i<5120;i++){
		  		 TxData[i] = indx;
		}
		indx++;
	}

	//se declanseaza la finalul transferului complet. Rescrie a doua jumatate cu indx
	// incrementeaza iar, apoi daca indx >=60 opreste DMA cu HAL_UART_DMAStop
	void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
		for(uint32_t i = 5120;i<10240;i++){
			TxData[i] = indx;
		}
		indx++;

		if(indx>=60){
			HAL_UART_DMAStop(&huart2);
		}
		isSent = 1;
		countinterrupt ++;
	}





	///------------ proiect de receptie a datelor trimise prin UART catre placa -----------------
	// modificari facute in CubeMX
	// dezactivez DMA si las active doar intreruperile
	// vreau sa ma folosesc de intreruperi si sa vad cum functioneaza astea

	uint8_t FinalData[20];
	uint8_t RxData[20];
	uint8_t temp[2];
	int indx2  = 0;
	//am nevoie de o functie de callback
	// intreruperea nu se activeaza (trigger) cand nr de octeti precizati nu se primesc
	// avantajul e acela ca pricesorul nu e blocat in timpul asteptarii datelor
	// restul codului se executa obisnuit
	// cand nr de octeti e egal cu cel care tb primiti, se face trigger la intrerupere
	// la aruncarea intreruperii, se va executa functia de callback (care efectiv executa ce trebuie sa execute
	// la activarea acelei intreruperi)
	// --> datele se vor procesa inauntrul functiei de callback
	// --> momentan nu procesez date acum, deci o las pentru mai incolo

	void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
		memcpy(RxData+indx2, temp, 1);
		if(++indx2>=20)
			indx2 = 0;

		HAL_UART_Receive_IT(&huart2, temp, 1);
	}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

//  	 for(uint32_t i = 0;i<10240;i++){
//  		 TxData[i] = i&(0xff);
//  	 }

//  	 HAL_UART_Transmit_DMA(&huart2, TxData, 10240);		// efectiv aici dau start si DMA face singur treaba
  	 	 	 	 	 	 	 	 	 	 	 	 	 	 //neconditionat de CPU
  	 //DMA se opreste singur de indata ce se adeveresc conditiile interne

  	  HAL_UART_Receive_IT(&huart2, temp, 1);
  	  // apelul de functie de mai sus trimite o intrerupere cand numarul de octeti precizati
  	  //este primit

  	  //avantajul este ca procesorul nu este blocat in tot acest timp, asa cum am vazut
  	  //la functia HAL_UART_Receive(..........)



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

//	  if(isSent ==1){
//	  		  	  HAL_UART_Transmit_IT(&huart2, TxData, 10240);
//	  		  isSent=0;
//	  	  }

//	  if(isSent ==1){
//		  HAL_UART_Transmit_DMA(&huart2, TxData, 10240);
//		  isSent=0;
//	  }

	  //HAL_UART_Transmit(&huart2, TxData, 10240, HAL_MAX_DELAY);
//	  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
//	  HAL_Delay(1000);
//	  countloop++;


	  //// ---------------- proiect de receptie date trimise UART

	  //HAL_UART_Receive(&huart2, RxData, 5, HAL_MAX_DELAY);
	  // hal uart receive receptioneaza date in modul blocant
	  //timeout-ul joaca un rol important aici --> functia asteapta doar pentru perioada asta de timp
	  //daca cei 5 octeti nu sun receptionati in cele 5 secunde, trec mai departe (functia intra in timeout)

	  //functia de mai sus nu trece mai departe decat atunci cand bufferul de 5 octeti
	  //pe care il asteapta este umplut. Functia sta atat timp cat ii spun sa stea (1 secunda
	  //sau HAL_MAX_DELAY, adica pana se umple bufferul

	  //Modul acesta de lucru e foarte util in alicatii rtos, cand pot crea un proces
	  //specializat doar pe receptia de date

	  //acum nu folosim RTOS, o sa comentez

	  // ------ stocare single data bytes foloisnd intreruperi


	  if(temp[0]=='\n'){
		  memcpy(FinalData, RxData, indx);
		  indx = 0;
	  		}

	  // in partea asta o sa las blinking LED sa funcitoneze inca, pentru monitorizare
	  HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
	  HAL_Delay(1000);

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
