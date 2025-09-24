/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "flash.h"
#include "stdarg.h"
#include "stdio.h"
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
SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define ADDR_APP_PROGRAM   0x0800C800U   
#define START_PAGE         50            
#define END_PAGE           127          
#define BLOCK_SIZE         256

/* Typedef */
typedef void (*pFunction)(void);

FATFS fs;
FIL fil;
FRESULT fr;
UINT br;
uint8_t firmware_buffer[BLOCK_SIZE];
char buff[150];

void HAL_PRINTF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    memset(buff, 0, sizeof(buff));
    vsnprintf(buff, sizeof(buff), format, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)buff, strlen(buff), 1000);
}

/* Flash erase pages */
void erase_page_by_user(uint8_t start, uint8_t stop) {
    HAL_PRINTF("Erasing flash pages %d to %d...\n", start, stop - 1);
    HAL_FLASH_Unlock();
    
    for (uint8_t i = start; i < stop; i++) {
        uint32_t addr = 0x08000000 + 1024 * i;
        flash_erase(addr);
    }
    
    HAL_PRINTF("Flash erase completed!\n");
}

void verify_application(void) {
    uint32_t* app_addr = (uint32_t*)ADDR_APP_PROGRAM;
    
    HAL_PRINTF("\n=== APPLICATION VERIFICATION ===\n");
    HAL_PRINTF("Application address: 0x%08X\n", ADDR_APP_PROGRAM);
    HAL_PRINTF("Stack pointer: 0x%08lX\n", app_addr[0]);
    HAL_PRINTF("Reset handler: 0x%08lX\n", app_addr[1]);
    
    if (app_addr[0] >= 0x20000000 && app_addr[0] <= 0x20005000) {
        HAL_PRINTF("Stack pointer: VALID\n");
    } else {
        HAL_PRINTF("Stack pointer: INVALID!\n");
    }
    
    if (app_addr[1] >= ADDR_APP_PROGRAM && app_addr[1] <= 0x08010000) {
        HAL_PRINTF("Reset handler: VALID\n");
    } else {
        HAL_PRINTF("Reset handler: INVALID!\n");
    }
    
    HAL_PRINTF("First 32 bytes of application:\n");
    for(int i = 0; i < 8; i++) {
        HAL_PRINTF("0x%08lX: 0x%08lX\n", ADDR_APP_PROGRAM + i*4, app_addr[i]);
    }
    
    if (app_addr[0] != 0xFFFFFFFF && app_addr[1] != 0xFFFFFFFF && 
        app_addr[0] != 0x00000000 && app_addr[1] != 0x00000000) {
        HAL_PRINTF("Application appears to be flashed correctly\n");
    } else {
        HAL_PRINTF("WARNING: Application may not be valid!\n");
    }
    
    HAL_PRINTF("=== END VERIFICATION ===\n\n");
}

void enter_to_application(void) {
    uint32_t* app_addr = (uint32_t*)ADDR_APP_PROGRAM;
    
    HAL_PRINTF("Preparing to jump to application...\n");
    HAL_PRINTF("Final check - SP: 0x%08lX, PC: 0x%08lX\n", app_addr[0], app_addr[1]);
    
    if (app_addr[0] < 0x20000000 || app_addr[0] > 0x20005000) {
        HAL_PRINTF("ERROR: Invalid stack pointer! Aborting jump.\n");
        return;
    }
    
    if (app_addr[1] < ADDR_APP_PROGRAM || app_addr[1] > 0x08010000) {
        HAL_PRINTF("ERROR: Invalid reset handler! Aborting jump.\n");
        return;
    }
    
    HAL_PRINTF("Starting application...\n");
    HAL_Delay(500);
    
    __disable_irq();
    
    HAL_UART_DeInit(&huart3); 
    HAL_SPI_DeInit(&hspi1);    
    HAL_RCC_DeInit();
    HAL_DeInit();
    
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    
    for (int i = 0; i < 8; i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    
    SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
                    SCB_SHCSR_BUSFAULTENA_Msk |
                    SCB_SHCSR_MEMFAULTENA_Msk);
    
    SCB->VTOR = ADDR_APP_PROGRAM;
    
    __set_MSP(app_addr[0]);
    pFunction app_entry = (pFunction)(app_addr[1]);
    app_entry();
}

uint8_t load_firmware_from_sdcard(const char* filename) {
    uint32_t write_address = ADDR_APP_PROGRAM;
    uint32_t total_bytes_written = 0;
    
    HAL_PRINTF("Starting firmware update...\n");
    
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK) {
        HAL_PRINTF("SD mount failed: %d\n", fr);
        return 0;
    }
    HAL_PRINTF("SD card mounted OK\n");
    
    fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        HAL_PRINTF("File open failed: %d\n", fr);
        return 0;
    }
    
    uint32_t file_size = fil.fsize;
    HAL_PRINTF("Firmware size: %lu bytes\n", file_size);
    
    if (file_size == 0) {
        HAL_PRINTF("File is empty!\n");
        f_close(&fil);
        return 0;
    }
    
    if (file_size > 14336) { 
        HAL_PRINTF("WARNING: File too large (%lu bytes > 14KB)\n", file_size);
    }
    
    erase_page_by_user(START_PAGE, END_PAGE);
    
    HAL_PRINTF("Writing firmware to flash...\n");
    
    while (total_bytes_written < file_size) {
        fr = f_read(&fil, firmware_buffer, BLOCK_SIZE, &br);
        if (fr != FR_OK || br == 0) {
            HAL_PRINTF("Read error: %d\n", fr);
            break;
        }
        
        flash_write(write_address, firmware_buffer, br);
        
        write_address += br;
        total_bytes_written += br;
        
        if (total_bytes_written % 1024 == 0 || total_bytes_written == file_size) {
            HAL_PRINTF("Progress: %lu/%lu bytes\n", total_bytes_written, file_size);
        }
    }
    
    f_close(&fil);
    HAL_FLASH_Lock();
    
    if (total_bytes_written == file_size) {
        HAL_PRINTF("Firmware update SUCCESS!\n");
        
        verify_application();
        
        return 1;
    } else {
        HAL_PRINTF("Update FAILED! %lu/%lu bytes\n", total_bytes_written, file_size);
        return 0;
    }
}

uint8_t count_d=0;


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
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
//	strcpy(file_name, "logan.txt");
//	strcpy(file_name1, "main.bin");
//	sdcard_logan_read();


		HAL_PRINTF("\n=== STM32 SD Card Bootloader v2.0 ===\n");
    HAL_PRINTF("Bootloader running at: 0x%08lX\n", (uint32_t)main);
    HAL_PRINTF("Application will be loaded at: 0x%08X\n", ADDR_APP_PROGRAM);
    HAL_PRINTF("Available app space: %d KB\n", (END_PAGE - START_PAGE));
    HAL_PRINTF("Press PB1 to update firmware from main.bin\n");
    
    uint32_t* app_check = (uint32_t*)ADDR_APP_PROGRAM;
    if (app_check[0] != 0xFFFFFFFF && app_check[1] != 0xFFFFFFFF) {
        HAL_PRINTF("Existing application detected at 0x%08X\n", ADDR_APP_PROGRAM);
        HAL_PRINTF("SP: 0x%08lX, PC: 0x%08lX\n", app_check[0], app_check[1]);
    } else {
        HAL_PRINTF("No application found - flash is empty\n");
    }
    
    HAL_PRINTF("Bootloader ready!\n\n");



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
            HAL_Delay(50); 
            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
                HAL_PRINTF("\n*** PB1 PRESSED - Starting Update ***\n");
                
                if (load_firmware_from_sdcard("main.bin")) {
                    HAL_PRINTF("*** UPDATE SUCCESSFUL ***\n");
                    
                    HAL_PRINTF("Press PB1 again within 5 seconds to start application\n");
                    HAL_PRINTF("Or wait to return to bootloader mode\n");
                    
                    uint32_t start_time = HAL_GetTick();
                    uint8_t second_press = 0;
                    
                    while ((HAL_GetTick() - start_time) < 5000) {
                        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
                            HAL_Delay(50);
                            if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
                                second_press = 1;
                                break;
                            }
                        }
                        HAL_Delay(100);
                    }
                    
                    if (second_press) {
                        HAL_PRINTF("Second press detected - starting application...\n");
                        enter_to_application();
                    } else {
                        HAL_PRINTF("Timeout - staying in bootloader\n");
                    }
                    
                } else {
                    HAL_PRINTF("*** UPDATE FAILED ***\n");
                    HAL_PRINTF("Check SD card and try again\n");
                }
                
                while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1) == GPIO_PIN_RESET) {
                    HAL_Delay(10);
                }
                HAL_PRINTF("Ready for next operation\n\n");
            }
        }

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
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
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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

#ifdef  USE_FULL_ASSERT
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
