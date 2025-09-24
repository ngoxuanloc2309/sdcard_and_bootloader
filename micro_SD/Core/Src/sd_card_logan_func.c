/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    sdcard_func.c
  * @brief   SD card functions implementation
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "sd_card_logan_func.h"

/* External variables --------------------------------------------------------*/
extern UART_HandleTypeDef huart3;

/* Private variables ---------------------------------------------------------*/
uint8_t hex_buffer[MAX_HEX_SIZE];
char uart_buff[UART_BUFFER_SIZE];
uint32_t file_bytes_read = 0;
uint32_t file_lines_count = 0;

/* External variables --------------------------------------------------------*/
extern FATFS fs;  // Ðu?c khai báo trong main.c

/* Private function prototypes -----------------------------------------------*/
static void HAL_PRINTF(const char* format, ...);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Printf function for UART output
 * @param  format: Printf format string
 * @param  ...: Variable arguments
 * @retval None
 */
static void HAL_PRINTF(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    memset(uart_buff, 0, sizeof(uart_buff));
    vsnprintf(uart_buff, sizeof(uart_buff), format, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)uart_buff, strlen(uart_buff), 1000);
}

/* Public functions ----------------------------------------------------------*/

/**
 * @brief  Initialize SD card and mount filesystem
 * @retval SD_Result_t status
 */
SD_Result_t sdcard_init(void)
{
    FRESULT fr;
    
    HAL_PRINTF("\n=== SD Card Initialization ===\n");
    
    fr = f_mount(&fs, "", 1);
    HAL_PRINTF("f_mount: %d\n", fr);
    
    if (fr != FR_OK) {
        HAL_PRINTF("ERROR: Mount failed!\n");
        return SD_MOUNT_ERROR;
    }
    
    HAL_PRINTF("SD Card mounted successfully!\n");
    return SD_OK;
}

/**
 * @brief  Read hex file from SD card (Logan's function)
 * @param  filename: Name of hex file to read
 * @retval SD_Result_t status (SD_OK if success)
 */
SD_Result_t sdcard_logan_read(const char* filename)
{
    FIL fil;
    FRESULT fr;
    UINT br;
    DWORD file_size;
    
    HAL_PRINTF("\n=== Logan's Hex File Reader ===\n");
    HAL_PRINTF("Reading file: %s\n", filename);
    
    // Open hex file
    fr = f_open(&fil, filename, FA_READ);
    HAL_PRINTF("f_open hex read: %d\n", fr);
    
    if (fr != FR_OK) {
        HAL_PRINTF("ERROR: Cannot open %s file!\n", filename);
        return SD_FILE_NOT_FOUND;
    }
    
    // Get file size
    file_size = f_size(&fil);
    HAL_PRINTF("Hex file size: %lu bytes\n", file_size);
    
    if (file_size >= MAX_HEX_SIZE) {
        HAL_PRINTF("ERROR: File too large! Max: %d bytes\n", MAX_HEX_SIZE);
        f_close(&fil);
        return SD_FILE_TOO_LARGE;
    }
    
    // Clear buffer
    memset(hex_buffer, 0, sizeof(hex_buffer));
    
    // Read entire file
    fr = f_read(&fil, hex_buffer, file_size, &br);
    HAL_PRINTF("f_read hex: %d, br=%u\n", fr, br);
    
    if (fr != FR_OK) {
        HAL_PRINTF("ERROR: Read failed!\n");
        f_close(&fil);
        return SD_READ_ERROR;
    }
    
    // Close file
    f_close(&fil);
    
    // Null terminator (ch? d? debug, bootloader không c?n)
    if (br < MAX_HEX_SIZE) {
        hex_buffer[br] = 0;
    }
    file_bytes_read = br;
    
    // Count lines
    file_lines_count = 0;
    for (uint32_t i = 0; i < br; i++) {
        if (hex_buffer[i] == '\n') file_lines_count++;
    }
    
    HAL_PRINTF("Successfully read %u bytes, %lu lines\n", br, file_lines_count);
    HAL_PRINTF("Buffer usage: %.1f%%\n", (float)br * 100 / MAX_HEX_SIZE);
    
    return SD_OK;
}

/**
 * @brief  Write data to file on SD card (Logan's function)
 * @param  size: Size of data to write
 * @param  name_file: Filename to write to
 * @param  data: Data to write
 * @retval SD_Result_t status (SD_OK if success)
 */
SD_Result_t sdcard_logan_write(uint32_t size, const char* name_file, const char* data)
{
    FIL fil;
    FRESULT fr;
    UINT bw;
    
    HAL_PRINTF("\n=== Logan's File Writer ===\n");
    HAL_PRINTF("Writing to: %s\n", name_file);
    HAL_PRINTF("Data size: %lu bytes\n", size);
    
    // Open file for writing (create or overwrite)
    fr = f_open(&fil, name_file, FA_CREATE_ALWAYS | FA_WRITE);
    HAL_PRINTF("f_open write: %d\n", fr);
    
    if (fr != FR_OK) {
        HAL_PRINTF("ERROR: Cannot open file for writing!\n");
        return SD_WRITE_ERROR;
    }
    
    // Write data
    fr = f_write(&fil, data, size, &bw);
    HAL_PRINTF("f_write: %d, bw=%u\n", fr, bw);
    
    if (fr != FR_OK || bw != size) {
        HAL_PRINTF("ERROR: Write failed!\n");
        f_close(&fil);
        return SD_WRITE_ERROR;
    }
    
    // Sync and close
    f_sync(&fil);
    f_close(&fil);
    
    HAL_PRINTF("Successfully written %u bytes to %s\n", bw, name_file);
    return SD_OK;
}

/**
 * @brief  Get file information
 * @param  filename: Name of file to check
 * @param  file_size: Pointer to store file size
 * @retval SD_Result_t status
 */
SD_Result_t sdcard_get_file_info(const char* filename, DWORD* file_size)
{
    FIL fil;
    FRESULT fr;
    
    fr = f_open(&fil, filename, FA_READ);
    if (fr != FR_OK) {
        return SD_FILE_NOT_FOUND;
    }
    
    *file_size = f_size(&fil);
    f_close(&fil);
    
    return SD_OK;
}

/**
 * @brief  Print hex buffer content (first N bytes)
 * @param  num_bytes: Number of bytes to print (0 = all)
 * @retval None
 */
void sdcard_print_hex_buffer(uint32_t num_bytes)
{
    uint32_t bytes_to_print;
    
    if (num_bytes == 0 || num_bytes > file_bytes_read) {
        bytes_to_print = file_bytes_read;
    } else {
        bytes_to_print = num_bytes;
    }
    
    HAL_PRINTF("\n=== Hex Buffer Content (%lu bytes) ===\n", bytes_to_print);
    
    for (uint32_t i = 0; i < bytes_to_print; i++) {
        HAL_PRINTF("%c", (char)hex_buffer[i]);
        
        // Add line break every 80 characters for readability
        if ((i + 1) % 80 == 0) {
            HAL_PRINTF("\n");
        }
    }
    
    if (bytes_to_print % 80 != 0) {
        HAL_PRINTF("\n");
    }
}

/**
 * @brief  Print file statistics
 * @retval None
 */
void sdcard_print_stats(void)
{
    HAL_PRINTF("\n=== File Statistics ===\n");
    HAL_PRINTF("Total bytes: %lu\n", file_bytes_read);
    HAL_PRINTF("Total lines: %lu\n", file_lines_count);
    HAL_PRINTF("Buffer usage: %.1f%% (%lu/%d)\n", 
               (float)file_bytes_read * 100 / MAX_HEX_SIZE, 
               file_bytes_read, MAX_HEX_SIZE);
    
    // Show first line
    if (file_bytes_read > 0) {
        HAL_PRINTF("First line: ");
        for (uint32_t i = 0; i < file_bytes_read && hex_buffer[i] != '\n' && hex_buffer[i] != '\r'; i++) {
            HAL_PRINTF("%c", (char)hex_buffer[i]);
        }
        HAL_PRINTF("\n");
    }
}