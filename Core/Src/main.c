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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "ring_buffer.h"
#include "uart_driver.h"
#include "i2c.h"
#include "freertos_tasks.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
DMA_HandleTypeDef hdma2;

#define NUM_CH   2
#define N_FRAMES 2
static volatile uint16_t adc_buf[N_FRAMES * NUM_CH];

uint8_t i2c_active_devices[I2C_NUM_VALID_ADDR];
// layout: [A0, B0, A1, B1]  -> frame 0 = {A0,B0}, frame 1 = {A1,B1}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart2;

/* Definitions for defaultTask */
//osThreadId_t defaultTaskHandle;
//const osThreadAttr_t defaultTask_attributes = {
//  .name = "defaultTask",
//  .stack_size = 128 * 4,
//  .priority = (osPriority_t) osPriorityNormal,
//};
/* USER CODE BEGIN PV */
volatile uint32_t start;
volatile uint32_t prev;
volatile uint32_t cycle_count;
volatile uint8_t cycle_flag = 0;


volatile uint8_t num = 0;

volatile bool RX_receive_ready = 0;
volatile bool RX_overrun_error = 0;
volatile bool RX_buffer_overflow = 0;

Ring_Buffer_t ring_buffer;
Ring_Buffer_t tx_buffer;
Ring_Buffer_t rx_buffer;


uint8_t storage[BUFFER_SIZE];
uint8_t tx_storage[BUFFER_SIZE];
uint8_t rx_storage[BUFFER_SIZE];

typedef struct {
	int integer_part;
	int fractional_part;
} Voltages_t;

unsigned int adc_group_sequence[NUM_CH] = {1,4};
volatile Voltages_t converted_voltage_vals[NUM_CH * N_FRAMES];
volatile bool adc_conver_ready_buf1 = false;
volatile bool adc_conver_ready_buf2 = false;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_CAN1_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART4_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
void StartDefaultTask(void *argument);

/* USER CODE BEGIN PFP */

int _write(int file, char *ptr, int len);

void start_DWT_cycle_counter(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t get_cycle_count(void) {
	return DWT->CYCCNT;
}

// FreeRTOS function
void vToggle_GPIO(void* params) {
	Pack_GPIO_Params_t* config = (Pack_GPIO_Params_t*)params;

	for(;;) { // FreeRTOS starts from here next time
		HAL_GPIO_TogglePin(config->GPIOx,config->GPIO_Pin);
		vTaskDelay(pdMS_TO_TICKS(config->Delay));
	}
}

// FreeRTOS function
void vBusy_Wait(void* params) { // The function stack is created once only
	uint32_t delay = *(uint32_t*)params;

	uint32_t ms_to_cycles = delay * (SystemCoreClock / 1000); // this can overflow for > 23s

	for(;;) {
		uint32_t start = get_cycle_count();
		while((get_cycle_count() - start) < ms_to_cycles);
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
//	uint32_t start = DWT->CYCCNT;
//
//	while((DWT->CYCCNT) < (start + 90000000)) {} // delay of 500ms

}

void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim) {
//	uint32_t record = DWT->CYCCNT;
//	cycle_count = record - prev; // wrap arounds are handled auto
//	prev = record;
//	cycle_flag = 1;
//	HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_5);

//	if(rb_push(&ring_buffer,num)) ++num;
//	else printf("Full\r\n");
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {


}

// DMA has read first half of the buffer, begins to fill in the second half
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) { // Later this ISR will just call a FreeRTOS task
//	start = DWT->CYCCNT;

	double voltage;

	for (int i=0; i<NUM_CH; i++) { // Keep NUM_CH low for now
		voltage = adc_buf[i] * (3.3/4095.0); // Note I did 33/4095 but the compiler treats the two numbers as integers -> 0

		// Extract the whole number components
		converted_voltage_vals[i].integer_part = (int)voltage;

		// Isolate and scale the fractional component (3 decimal places)
		converted_voltage_vals[i].fractional_part = (int)((voltage - converted_voltage_vals[i].integer_part) * 1000);

	}

	adc_conver_ready_buf1 = true;


}

// DMA has read second half of the buffer, begins to fill in the first half
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
//	cycle_count = DWT->CYCCNT - start; // wrap arounds are handled auto
//	cycle_flag = 1;

	double voltage;

	for (int i=((NUM_CH * N_FRAMES) / 2); i<(NUM_CH * N_FRAMES); i++) { // Keep NUM_CH low for now
		voltage = adc_buf[i] * (3.3/4095.0); // Note I did 33/4095 but the compiler treats the two numbers as integers -> 0

		// Extract the whole number components
		converted_voltage_vals[i].integer_part = (int)voltage;

		// Isolate and scale the fractional component (3 decimal places)
		converted_voltage_vals[i].fractional_part = (int)((voltage - converted_voltage_vals[i].integer_part) * 1000);

	}

	adc_conver_ready_buf2 = true;

}

void start_interrupts(void) {
//	  HAL_ADC_Start_IT(&hadc1);

	 // Make sure to use length in number of ADC CONVERSIONS, not frames
	  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buf, N_FRAMES * NUM_CH);

	  HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1);
	  // Enable update interrupt and disable Capture/Compare interrupt (for now)
	  TIM2->DIER |= TIM_DIER_UIE;
	  TIM2->DIER &= ~TIM_DIER_CC1IE;


	  enable_uart_rx_interrupt();
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_ADC1_Init();
  MX_CAN1_Init();
  MX_I2C1_Init();
  MX_UART4_Init();
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  setvbuf(stdout, NULL, _IONBF, 0);
  start_DWT_cycle_counter();

  rb_init(&ring_buffer, storage, BUFFER_SIZE);
  rb_init(&tx_buffer, tx_storage, BUFFER_SIZE);
  rb_init(&rx_buffer, rx_storage, BUFFER_SIZE);

  uint8_t holder;
  uint8_t uart_receiver_buffer[BUFFER_SIZE];



//  start_interrupts();


  uint8_t char_array[] = {'H','E','L','L','O','\r','\n'};
  uint32_t rough_timer = HAL_GetTick();

  /* USER CODE END 2 */

  /* Init scheduler */
  //osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  //defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

//  static Pack_GPIO_Params_t PC2 = {.GPIOx = GPIOC, .GPIO_Pin = GPIO_PIN_2, .Delay = 500};
//  static Pack_GPIO_Params_t PC3 = {.GPIOx = GPIOC, .GPIO_Pin = GPIO_PIN_3, .Delay = 1000};
//  static uint32_t busy_task_delay = 6000;
//
//  xTaskCreate(vToggle_GPIO,
//		  	  "toggle_pin_PC2_fast",
//			  configMINIMAL_STACK_SIZE, // make the stack size much bigger (measured in words)
//			  (void*)&PC2,
//			  1,
//			  NULL
//		  	  );
//
//  xTaskCreate(vToggle_GPIO,
//		  	  "toggle_pin_PC3_slow",
//			  configMINIMAL_STACK_SIZE,
//			  (void*)&PC3,
//			  1,
//			  NULL
//		  	  );
//
////   The function vBusy_Wait had a bug in it
//  xTaskCreate(vBusy_Wait, // in milliseconds
//		  	  "task_hog",
//			  configMINIMAL_STACK_SIZE,
//			  (void*)&busy_task_delay,
//			  2,
//			  NULL
//		  	  );

//  xTaskCreate(vMainControl,
//		  	  "main_control_task",
//		      configMINIMAL_STACK_SIZE,
//			  )

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  //osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */


  /* Start scheduler */
  vTaskStartScheduler();


  /* We should never get here as control is now taken by the scheduler */

  while (1)
  {
    /* USER CODE END WHILE */

//	  HAL_Delay(500);
//	  uart_write(&tx_buffer,char_array,7);

	  if (HAL_GetTick() - rough_timer > 1000) {
//		  uart_write(&tx_buffer, char_array, 7);
		  rough_timer = HAL_GetTick();
//	  }


//		  if (cycle_flag) {
//			  cycle_flag = 0;
//			  printf("%lu\r\n", cycle_count);
//		  }

		  if (adc_conver_ready_buf1) { // Use better name like adc_half_compl

			  adc_conver_ready_buf1 = false;

			  printf("HalfCplt - ");
			  	for (int i=0; i<NUM_CH; i++) {
					// Print without requiring runtime float libraries
					printf("Ch:%u, %d.%03d V\t", adc_group_sequence[i]
													, converted_voltage_vals[i].integer_part
													, converted_voltage_vals[i].fractional_part);

			  	}
			  	printf("\r\n");

		  }
		  if (adc_conver_ready_buf2) {

			  adc_conver_ready_buf2 = false;

			  printf("FullCplt - ");
			  	for (int i=((NUM_CH * N_FRAMES) / 2); i<(NUM_CH * N_FRAMES); i++) {
					// Print without requiring runtime float libraries
					printf("Ch:%u, %d.%03d V\t", adc_group_sequence[i-NUM_CH]
													, converted_voltage_vals[i].integer_part
													, converted_voltage_vals[i].fractional_part);

			  	}
			  	printf("\r\n");

		  }
	  }


	  if (RX_receive_ready) {
		  // Set RX_receive_ready to false, use critical section
			__disable_irq();
			RX_receive_ready = false;
			__enable_irq();

		  uint8_t rx_data;
		  uart_read(&rx_buffer,&rx_data);

		  printf("\tRECEIVED: %c\r\n",(char)rx_data);

		  //uint32_t data_sent = uart_write(&tx_buffer, char_array, 7);
	  }

	  if (RX_overrun_error) { // ORE
			__disable_irq();
			RX_overrun_error = false;
			__enable_irq();

		  printf("OVERRUN ERROR HAS OCCURED\r\n");
		  // Could add more stuff here
	  }

	  if (RX_buffer_overflow) {
			__disable_irq();
			RX_buffer_overflow = false;
			__enable_irq();

		  printf("BUFFER OVERFLOW HAS OCCURED\r\n");
		  // Could add more stuff here
	  }
    /* USER CODE BEGIN 3 */
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
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV8;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */


  // The ADC has a speed of 90 MHz/4 = 22.5 MHz
  // Note the ADC VREF+ is tied to VDD (3.3V), VREF- is tied to VSS (0V)



  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T2_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = NUM_CH; // STMF446 doesn't have oversampling unit, need to do it manually
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 7;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Rank = 8;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */


  // Manually add channels here (not relying on CubeMX). Using 2 channels
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 2;
  sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  // Enable interrupt
  HAL_NVIC_SetPriority(ADC_IRQn,5,0);
  HAL_NVIC_EnableIRQ(ADC_IRQn);

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 16;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_1TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_1TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */
  HAL_TIM_OC_MspInit (&htim2);
  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 45; // APB1 is 45MHz
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999; // Value to be loaded
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_TIMING;
  sConfigOC.Pulse = 999; // interrupt every of 1 ms
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  // Set update request source to counter overflow to enable update interrupt
  TIM2->CR1 |= TIM_CR1_URS;

  // Make sure UEV is enabled
  TIM2->CR1 &= ~TIM_CR1_UDIS;


  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

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
  	  // The interrupt has been enabled in HAL_UART_MspInit. Note UART's TX and RX share same interrupt vector
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC2 */
  GPIO_InitStruct.Pin = GPIO_PIN_2;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PC3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */ // For the green led
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);


  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /*Configure GPIO pin : PC13 (the user button) <- delete this later if setting using cubemx */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn,8,0); // Set PC13 (the blue button) to external interrupt
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
//#ifdef __GNUC__ // GNU compiler (gcc)
//#define PUTCHAR_PROTOTYPE int __io_putchar(int ch) //gcc expects this
//#else
//#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f) // other compilers expect this
//#endif
//
//PUTCHAR_PROTOTYPE {
//	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
//	return ch;
//}



int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}









/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */
//  if (htim->Instance == TIM2) {
//	  printf("Update TIM2 Working\r\n");
//  }
  /* USER CODE END Callback 1 */
}

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
