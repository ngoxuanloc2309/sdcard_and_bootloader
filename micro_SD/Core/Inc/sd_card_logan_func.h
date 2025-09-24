/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    sdcard_func.h
  * @brief   Header file for SD card functions
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __SDCARD_FUNC_H
#define __SDCARD_FUNC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Exported types ------------------------------------------------------------*/
typedef enum {
    SD_OK = 0,
    SD_ERROR = -1,
    SD_FILE_NOT_FOUND = -2,
    SD_FILE_TOO_LARGE = -3,
    SD_MOUNT_ERROR = -4,
    SD_WRITE_ERROR = -5,
    SD_READ_ERROR = -6
} SD_Result_t;

/* Exported constants --------------------------------------------------------*/
#define MAX_HEX_SIZE 8192        // 8KB buffer for hex file
#define UART_BUFFER_SIZE 128     // UART printf buffer
#define MAX_FILENAME_SIZE 32     // Maximum filename length

/* Exported variables --------------------------------------------------------*/
extern uint8_t hex_buffer[MAX_HEX_SIZE];
extern uint32_t file_bytes_read;
extern uint32_t file_lines_count;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief  Initialize SD card and mount filesystem
 * @retval SD_Result_t status
 */
SD_Result_t sdcard_init(void);

/**
 * @brief  Read hex file from SD card (Logan's function)
 * @param  filename: Name of hex file to read
 * @retval SD_Result_t status (SD_OK if success)
 */
SD_Result_t sdcard_logan_read(const char* filename);

/**
 * @brief  Write data to file on SD card (Logan's function)
 * @param  size: Size of data to write
 * @param  name_file: Filename to write to
 * @param  data: Data to write
 * @retval SD_Result_t status (SD_OK if success)
 */
SD_Result_t sdcard_logan_write(uint32_t size, const char* name_file, const char* data);

/**
 * @brief  Get file information
 * @param  filename: Name of file to check
 * @param  file_size: Pointer to store file size
 * @retval SD_Result_t status
 */
SD_Result_t sdcard_get_file_info(const char* filename, DWORD* file_size);

/**
 * @brief  Print hex buffer content (first N bytes)
 * @param  num_bytes: Number of bytes to print (0 = all)
 * @retval None
 */
void sdcard_print_hex_buffer(uint32_t num_bytes);

/**
 * @brief  Print file statistics
 * @retval None
 */
void sdcard_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* __SDCARD_FUNC_H */