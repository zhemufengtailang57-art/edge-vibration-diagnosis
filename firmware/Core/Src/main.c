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
#include "stdio.h"
#include "ssd1306.h"
#include "adxl345.h"
#include "ds18b20.h"
#include "w25q64.h"
#include "max3485.h"
#include "log.h"
#include "adxl345_dma.h"
#include "filter.h"
#include "fft_analyzer.h"
#include "modbus_slave.h"
#include "motor.h"
#include "ai_classifier.h"
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
I2C_HandleTypeDef hi2c1;

IWDG_HandleTypeDef hiwdg;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;

/* Definitions for sensorTask */
osThreadId_t sensorTaskHandle;
const osThreadAttr_t sensorTask_attributes = {
  .name = "sensorTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for watchdogTask */
osThreadId_t watchdogTaskHandle;
const osThreadAttr_t watchdogTask_attributes = {
  .name = "watchdogTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for modbusTask */
osThreadId_t modbusTaskHandle;
const osThreadAttr_t modbusTask_attributes = {
  .name = "modbusTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,  /* 最高优先级, 发送不被打断 */
};
/* USER CODE BEGIN PV */
static int16_t temp_ds18;
static uint8_t temp_flag = 0;
static FFT_Result fft_result;                     // FFT分析结果
static uint8_t  g_ai_class = 0;                   // AI分类: 0正常 1松动 2不平衡
static char  mqtt_json_buf[128];                  // 待发送的MQTT JSON
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_IWDG_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
void StartSensorTask(void *argument);
void StartDisplayTask(void *argument);
void StartwatchdogTask(void *argument);
void StartmodbusTask(void *argument);

/* USER CODE BEGIN PFP */

#ifdef __GNUC__
/* GCC��������ʵ��_write */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
#else

int fputc(int ch, FILE *f)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
#endif
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* FreeRTOS Tick钩子 (每1ms调用, 当前仅占位) */
void vApplicationTickHook(void)
{
}

/* FreeRTOS 栈溢出钩子 — 记录哪个任务爆栈了, 然后复位 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    uint8_t i;
    const char *p;

    taskDISABLE_INTERRUPTS();

    /* 用寄存器直写UART (LOG系统可能已损坏) */
    p = "FATAL: Stack overflow in task: ";
    for (i = 0; p[i]; i++)
    {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = p[i];
    }
    p = pcTaskName;
    for (i = 0; p[i]; i++)
    {
        while (!(USART2->SR & USART_SR_TXE));
        USART2->DR = p[i];
    }
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = '\r';
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = '\n';
    while (!(USART2->SR & USART_SR_TC));

    for( ;; );  /* 死循环, IWDG会负责复位 */
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
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_SPI2_Init();
  MX_USART3_UART_Init();
  /* MX_IWDG_Init() 已移到末尾 */
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  Log_Init();
  LOG_INFO("MAIN", "System Boot OK, SYSCLK=168MHz");

  SSD1306_Init();
  /* 启动画面 (128x64布局: 标题16px + V2.0大字16px + 三行自检结果8px, 无重叠) */
  SSD1306_ShowCN(16, 0, "\xE6\x8C\xAF\xE5\x8A\xA8\xE7\x9B\x91\xE6\xB5\x8B\xE7\xB3\xBB\xE7\xBB\x9F");  /* 振动监测系统 */
  SSD1306_ShowString(32, 16, "V2.0", 2);
  SSD1306_ShowString(0, 36, "ADXL ..", 1);
  SSD1306_ShowString(66, 36, "DS18 ..", 1);
  SSD1306_ShowString(0, 46, "FLASH ..", 1);
  SSD1306_ShowString(66, 46, "485 ..", 1);
  SSD1306_ShowString(0, 56, "MOTOR OK!", 1);
  SSD1306_Refresh();

  /* ADXL345 */
  uint8_t adxl_ret = ADXL345_Init();
  if (adxl_ret == 0)
  {
      LOG_INFO("ADXL345", "Init OK");
        /* 启动1kHz双缓冲采样 */
        ADXL_DMA_Init();
        /* 注: MA8+MED5滤波管线已从数据流移除 —— FFT与AI模型均基于原始采样
         * 特征训练, 插入滤波会改变频谱特征导致模型失效 (filter.c/h保留备查) */
        FFT_Analyzer_Init();   // ← FFT初始化
      SSD1306_ShowString(0, 36, "ADXL OK!", 1);
  }
  else
  {
      LOG_ERROR("ADXL345", "Init FAIL, ret=%d", adxl_ret);
      SSD1306_ShowString(0, 36, "ADXL FAIL", 1);
  }

  /* DS18B20 */
  uint8_t ds_ret = DS18B20_Init();
  if (ds_ret == 0)
  {
      LOG_INFO("DS18B20", "Init OK");
      SSD1306_ShowString(66, 36, "DS18 OK!", 1);
      DS18B20_StartConversion();
  }
  else
  {
      LOG_ERROR("DS18B20", "Init FAIL, ret=%d", ds_ret);
      SSD1306_ShowString(66, 36, "DS18 FAIL", 1);
  }

  /* ESP8266已弃用 → ESP32替代 (USART1, 连线相同) */
  LOG_INFO("ESP", "ESP8266 skipped, ESP32 ready on USART1");

  /* W25Q64 */
  uint8_t w25_ret = W25Q64_Init();
  if (w25_ret == 0)
  {
      LOG_INFO("W25Q64", "JEDEC OK");
      LOG_INFO("W25Q64", "Blackbox records: %d", Blackbox_GetCount());

      /* W25Q64 读写验证: 擦→写→读→校验 */
      {
          uint8_t wbuf[32], rbuf[32];
          LOG_INFO("W25Q64", "Sector erase test...");
          W25Q64_SectorErase(0);  /* 擦除扇区0 */
          W25Q64_WaitBusy();

          /* 写测试数据 */
          sprintf((char *)wbuf, "VIB_BLACKBOX_TEST_OK!");
          W25Q64_PageWrite(0, wbuf, 32);
          W25Q64_WaitBusy();

          /* 读回校验 */
          memset(rbuf, 0, sizeof(rbuf));
          W25Q64_Read(0, rbuf, 32);
          if (strcmp((char *)wbuf, (char *)rbuf) == 0)
          {
              LOG_INFO("W25Q64", "R/W test PASS");

              /* Dump黑匣子历史记录 */
              {
                  uint16_t cnt = Blackbox_GetCount();
                  LOG_INFO("W25Q64", "=== Blackbox Dump (%d records) ===", cnt);
                  for (uint16_t i = 0; i < cnt && i < 10; i++)  /* 最多显示10条 */
                  {
                      BlackboxRecord rec;
                      if (Blackbox_Read(i, &rec))
                          LOG_INFO("BBOX", "#%d: T=%ds RMS=%dmg Freq=%dHz Amp=%dmg Temp=%d.%dC Status=%d",
                                   i+1, rec.timestamp, rec.rms_mg, rec.freq_hz,
                                   rec.amp_mg, rec.temp_x10/10, rec.temp_x10%10, rec.status);
                  }
                  LOG_INFO("W25Q64", "=== End Blackbox ===");
              }
              SSD1306_ShowString(0, 46, "FLASH OK!", 1);
          }
          else
          {
              LOG_ERROR("W25Q64", "R/W test FAIL!");
              SSD1306_ShowString(0, 46, "FLASH FAIL", 1);
          }
      }
  }
  else
  {
      LOG_ERROR("W25Q64", "JEDEC FAIL, ret=%d", w25_ret);
      SSD1306_ShowString(0, 46, "FLASH FAIL", 1);
  }

  /* RS485 */
  RS485_Init();
  LOG_INFO("RS485", "Init OK");
  Modbus_Init();
  SSD1306_ShowString(66, 46, "485 OK!", 1);

  /* 电机驱动初始化 (不启动, 等命令) */
  Motor_Init();
  LOG_INFO("MOTOR", "TB6612 Init OK, PWM=1kHz");

  /* AI分类器初始化 */
  AI_Classifier_Init();
  LOG_INFO("AI", "Classifier ready (5->16->8->3, 259 params, int8)");

  SSD1306_Refresh();
  HAL_Delay(3000);
    /* 初始化完成后才启动看门狗 */
  MX_IWDG_Init();
  LOG_INFO("MAIN", "IWDG enabled, 16s timeout");

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* USER CODE BEGIN RTOS_MUTEX */
  /* SPI2互斥量: W25Q64 黑匣子 vs 健康快照 防总线冲突 */
  extern osMutexId_t spi2MutexHandle;
  spi2MutexHandle = osMutexNew(NULL);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of sensorTask */
  sensorTaskHandle = osThreadNew(StartSensorTask, NULL, &sensorTask_attributes);

  /* creation of displayTask */
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);

  /* creation of watchdogTask */
  watchdogTaskHandle = osThreadNew(StartwatchdogTask, NULL, &watchdogTask_attributes);

  /* creation of modbusTask */
  modbusTaskHandle = osThreadNew(StartmodbusTask, NULL, &modbusTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
  }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
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
  hi2c1.Init.ClockSpeed = 100;
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
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_128;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 83;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 83;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LED_Pin|GPIO_PIN_6|GPIO_PIN_7|RS485_DE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4|GPIO_PIN_0|GPIO_PIN_1, GPIO_PIN_SET);  // PA4=ADXL_CS(低), PA0=黄LED, PA1=红LED(高灭)

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, BUZZER_Pin|GPIO_PIN_2|W25Q64_CS_Pin|GPIO_PIN_5, GPIO_PIN_RESET);  // PB0=蜂鸣器(低), PB5=DS18B20

  /*Configure GPIO pins : LED_Pin PC6 PC7 RS485_DE_Pin */
  GPIO_InitStruct.Pin = LED_Pin|GPIO_PIN_6|GPIO_PIN_7|RS485_DE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA0 PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : BUZZER PB2 W25Q64_CS PB5 */
  GPIO_InitStruct.Pin = BUZZER_Pin|GPIO_PIN_2|W25Q64_CS_Pin|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartSensorTask */
/**
  * @brief  Function implementing the sensorTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void *argument)
{
  /* USER CODE BEGIN StartSensorTask */
  for(;;)
  {
      HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);

      /* ADXL345双缓冲 */
      ADXL_Buffer *adxl_buf = NULL;
      if (ADXL_DMA_GetReadyBuf(&adxl_buf))
      {
          /* FFT */
          {
              uint16_t cnt = adxl_buf->count;
              int16_t fft_data[128];
              for (uint16_t i = 0; i < cnt && i < 128; i++)
                  fft_data[i] = adxl_buf->data[i].x;
              FFT_Analyze(fft_data, cnt, &fft_result);
          }

          /* AI推理: 5特征 → 三工况分类 (结果经防抖后写入status) */
          uint8_t ai_class;
          {
              static uint8_t ai_initialized = 0;
              if (!ai_initialized) { AI_Classifier_Init(); ai_initialized = 1; }

              float ai_input[5];
              float ai_conf = 0.0f;
              ai_input[0] = fft_result.rms * 1000.0f;            /* RMS mG */
              ai_input[1] = fft_result.spectrum[1] * 500.0f;     /* bin1 = 7.8Hz */
              ai_input[2] = fft_result.spectrum[2] * 500.0f;     /* bin2 = 15.6Hz */
              ai_input[3] = fft_result.spectrum[6] * 500.0f;     /* bin6 = 46.9Hz */
              ai_input[4] = fft_result.spectrum[7] * 500.0f;     /* bin7 = 54.7Hz */
              if (fft_result.rms < 0.05f)  /* RMS<50mG: 电机停转, 不喂AI */
              {
                  ai_class = 0;       /* 停转=正常 */
                  ai_conf = 1.0f;
              }
              else
              {
                  ai_class = AI_Classify(ai_input, &ai_conf);
              }
              g_ai_class = ai_class;

              static uint8_t last_ai_class = 0xFF;
              if (ai_class != last_ai_class)
              {
                  static const char *cls_name[] = {"normal", "loose", "unbalance"};
                  LOG_INFO("AI", "Class=%s conf=%.1f%%", cls_name[ai_class], ai_conf * 100.0f);
                  last_ai_class = ai_class;
              }
          }

          /* 防抖: 连续3帧(600ms)一致才认定为稳定工况, 防止碰桌面的瞬时振动误判 */
          uint8_t status;
          static uint8_t stable_class = 0xFF;
          static uint8_t stable_cnt = 0;
          static uint8_t prev_stable = 0;
          if (ai_class == stable_class)
              stable_cnt++;
          else
          {
              stable_class = ai_class;
              stable_cnt = 1;
          }
          if (stable_cnt >= 3)
              status = stable_class;   /* 稳定工况生效 */
          else
              status = prev_stable;     /* 抖动期间保持上次稳定工况 */
          Modbus_UpdateRegs(
              (uint16_t)(fft_result.rms * 1000),
              (uint16_t)fft_result.freq,
              (uint16_t)(fft_result.amplitude * 1000),
              temp_ds18,
              Log_GetUptimeSec(),
              status);

          /* 频谱特征 → AI输入 (16频点, 0~117Hz, 幅值/2防饱和) */
          {
              uint16_t spec_mg[16];
              for (int i = 0; i < 16; i++)
              {
                  float v = fft_result.spectrum[i] * 500.0f;  /* /2防溢出 */
                  if (v > 65535.0f) v = 65535.0f;
                  spec_mg[i] = (uint16_t)v;
              }
              Modbus_UpdateSpectrum(spec_mg, 16);
          }

          /* MQTT JSON → ESP32 (USART1寄存器直写) */
          {
              int pos = sprintf(mqtt_json_buf,
                  "{\"rms\":%d,\"freq\":%d,\"amp\":%d,"
                  "\"temp\":%d.%d,\"up\":%lu,\"st\":%d}\n",
                  (int)(fft_result.rms * 1000),
                  (int)fft_result.freq,
                  (int)(fft_result.amplitude * 1000),
                  temp_ds18 / 10, (temp_ds18 < 0 ? -temp_ds18 : temp_ds18) % 10,
                  Log_GetUptimeSec(), status);
              for (int i = 0; i < pos; i++)
              {
                  while (!(USART1->SR & USART_SR_TXE));
                  USART1->DR = mqtt_json_buf[i];
              }
          }

          /* 三色LED告警 */
          if (status == 2) {
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_RESET);   // 红灯亮
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);     // 黄灯灭
          } else if (status == 1) {
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);     // 红灯灭
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET);   // 黄灯亮
          } else {
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);     // 红灯灭
              HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET);     // 黄灯灭
          }

          /* 工况切换事件记录: 防抖稳定且与上次工况不同才写一条
           * (事件型黑匣子: 进入异常记一条, 持续异常不重复刷) */
          if (stable_cnt >= 3 && stable_class != prev_stable)
          {
              uint8_t event_status = stable_class;
              prev_stable = stable_class;
              if (event_status >= 1 && fft_result.valid)
              {
                  BlackboxRecord rec;
                  rec.timestamp = Log_GetUptimeSec();
                  rec.rms_mg    = (uint16_t)(fft_result.rms * 1000);
                  rec.freq_hz   = (uint16_t)fft_result.freq;
                  rec.amp_mg    = (uint16_t)(fft_result.amplitude * 1000);
                  rec.temp_x10  = temp_ds18;
                  rec.peak_bin  = fft_result.peak_bin;
                  rec.status    = event_status;
                  rec.reserved  = 0;
                  uint16_t cnt = Blackbox_Write(&rec);
                  LOG_WARN("BBOX", "Event #%d recorded, RMS=%.2fg status=%d", cnt, fft_result.rms, event_status);
              }
          }

          adxl_buf->ready = 0;
          adxl_buf->count = 0;
      }

      /* DS18B20 */
      if (temp_flag == 0) temp_flag = 1;
      else if (temp_flag >= 4) {
          temp_ds18 = DS18B20_ReadTemp();
          DS18B20_StartConversion();
          temp_flag = 1;
      } else temp_flag++;

      /* 电机上电自动启动 60% (PCB有12V物理开关控制通断) */
      {
          static uint8_t motor_started = 0;
          if (!motor_started) {
              Motor_Forward(600);
              motor_started = 1;
              LOG_INFO("MOTOR", "Auto start 60%% duty");
          }
      }

      osDelay(200);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
* @brief Function implementing the displayTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
  for(;;)
  {
      /* 刷新OLED (I2C1仅OLED一个设备, displayTask独占, 无需互斥锁) */
      SSD1306_Clear();

      /* 布局规划: 中文16x16必须16px对齐, ASCII 8px
       * 工况 y=0-15 / RMS y=16-23 / 主频 y=24-31
       * 温度中文 y=32-47 + 数据y=36 / 运行中文 y=48-63 + 数据y=52
       */

      /* 第1行: 工况状态 (2字x=48居中, 3字x=40居中) */
      if (g_ai_class == 0)
          SSD1306_ShowCN(48, 0, "\xE6\xAD\xA3\xE5\xB8\xB8");              /* 正常 */
      else if (g_ai_class == 1)
          SSD1306_ShowCN(48, 0, "\xE6\x9D\xBE\xE5\x8A\xA8");              /* 松动 */
      else
          SSD1306_ShowCN(40, 0, "\xE4\xB8\x8D\xE5\xB9\xB3\xE8\xA1\xA1");  /* 不平衡 */

      /* 第2行: RMS */
      SSD1306_ShowString(0, 16, "RMS", 1);
      SSD1306_ShowNum(24, 16, (int)(fft_result.rms * 1000), 5, 1);
      SSD1306_ShowString(60, 16, "mG", 1);

      /* 第3行: 主频 */
      SSD1306_ShowString(0, 24, "F", 1);
      SSD1306_ShowNum(12, 24, (int)fft_result.freq, 3, 1);
      SSD1306_ShowString(36, 24, "Hz", 1);

      /* 第4行: 温度 (中文标签16px + 数据8px同行) */
      SSD1306_ShowCN(0, 32, "\xE6\xB8\xA9\xE5\xBA\xA6");   /* 温度 */
      if (temp_ds18 > 0)
      {
          SSD1306_ShowNum(40, 36, temp_ds18 / 10, 2, 1);
          SSD1306_ShowString(52, 36, ".", 1);
          SSD1306_ShowNum(58, 36, temp_ds18 % 10, 1, 1);
          SSD1306_ShowString(66, 36, "C", 1);
      }

      /* 第5行: 运行时间 */
      SSD1306_ShowCN(0, 48, "\xE8\xBF\x90\xE8\xA1\x8C");    /* 运行 */
      uint32_t up = Log_GetUptimeSec();
      SSD1306_ShowNum(40, 52, up / 3600, 2, 1);
      SSD1306_ShowString(52, 52, ":", 1);
      SSD1306_ShowNum(58, 52, (up % 3600) / 60, 2, 1);
      SSD1306_ShowString(70, 52, ":", 1);
      SSD1306_ShowNum(76, 52, up % 60, 2, 1);

      SSD1306_Refresh();

      osDelay(200);
  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartwatchdogTask */
/**
* @brief Function implementing the watchdogTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartwatchdogTask */
void StartwatchdogTask(void *argument)
{
  /* USER CODE BEGIN StartwatchdogTask */
  uint32_t last_log = 0;
  for(;;)
  {
      HAL_IWDG_Refresh(&hiwdg);
      if (Log_GetUptimeSec() - last_log >= 30)
      {
          /* 栈水印: 每个任务最少剩余栈空间(单位: 字=4字节) */
          UBaseType_t stk_s = uxTaskGetStackHighWaterMark(sensorTaskHandle);
          UBaseType_t stk_d = uxTaskGetStackHighWaterMark(displayTaskHandle);
          UBaseType_t stk_mb = uxTaskGetStackHighWaterMark(modbusTaskHandle);
          UBaseType_t stk_w = uxTaskGetStackHighWaterMark(watchdogTaskHandle);
          uint32_t heap_free = xPortGetFreeHeapSize();

          LOG_INFO("WDOG", "Uptime=%ds | Heap=%d/%d",
                   Log_GetUptimeSec(), heap_free, configTOTAL_HEAP_SIZE);
          LOG_INFO("WDOG", "Stack free(B): S=%d D=%d MB=%d W=%d",
                   stk_s*4, stk_d*4, stk_mb*4, stk_w*4);

          /* 任意任务栈<256B → 告警 */
          if (stk_s < 64 || stk_d < 64 || stk_mb < 64 || stk_w < 64)
              LOG_WARN("WDOG", "Low stack! May need increase");

          /* 健康快照写Flash (独立页, 每次覆盖不累积) */
          {
              extern osMutexId_t spi2MutexHandle;
              if (spi2MutexHandle) osMutexAcquire(spi2MutexHandle, osWaitForever);
              static uint8_t snap_first = 1;
              uint32_t snap_addr = 0x500000;  /* W25Q64末尾区域 */
              uint8_t  snap_buf[32];
              memset(snap_buf, 0, sizeof(snap_buf));
              memcpy(snap_buf, "HLTH", 4);
              snap_buf[4]  = (Log_GetUptimeSec() >> 24) & 0xFF;
              snap_buf[5]  = (Log_GetUptimeSec() >> 16) & 0xFF;
              snap_buf[6]  = (Log_GetUptimeSec() >> 8) & 0xFF;
              snap_buf[7]  =  Log_GetUptimeSec() & 0xFF;
              snap_buf[8]  = (heap_free >> 8) & 0xFF;
              snap_buf[9]  =  heap_free & 0xFF;
              snap_buf[10] = (stk_s * 4) >> 8;
              snap_buf[11] = (stk_s * 4) & 0xFF;
              snap_buf[12] = (stk_mb * 4) >> 8;
              snap_buf[13] = (stk_mb * 4) & 0xFF;
              if (snap_first) {
                  W25Q64_SectorErase(snap_addr);
                  W25Q64_WaitBusy();
                  snap_first = 0;
              }
              W25Q64_PageWrite(snap_addr, snap_buf, 32);
              W25Q64_WaitBusy();
              if (spi2MutexHandle) osMutexRelease(spi2MutexHandle);
          }

          last_log = Log_GetUptimeSec();
      }
      osDelay(500);
  }
  /* USER CODE END StartwatchdogTask */
}

/* USER CODE BEGIN Header_StartmodbusTask */
/**
* @brief Function implementing the modbusTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartmodbusTask */
void StartmodbusTask(void *argument)
{
  /* USER CODE BEGIN StartmodbusTask */
  for(;;)
  {
      Modbus_Poll();
      osDelay(1);
  }
  /* USER CODE END StartmodbusTask */
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
  if (htim->Instance == TIM4)
  {
    ADXL_DMA_TIM4_ISR();

    /* 中断里喂狗 (每1ms一次, 远比任务可靠) */
    static uint16_t iwdg_cnt = 0;
    if (++iwdg_cnt >= 500)   /* 500ms喂一次 */
    {
        iwdg_cnt = 0;
        HAL_IWDG_Refresh(&hiwdg);
    }
  }
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
