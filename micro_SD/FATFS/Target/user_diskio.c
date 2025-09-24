/* USER CODE BEGIN Header */
/**
 ******************************************************************************
  * @file    user_diskio.c
  * @brief   This file includes a diskio driver skeleton to be completed by the user.
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

#ifdef USE_OBSOLETE_USER_CODE_SECTION_0
/*
 * Warning: the user section 0 is no more in use (starting from CubeMx version 4.16.0)
 * To be suppressed in the future.
 * Kept to ensure backward compatibility with previous CubeMx versions when
 * migrating projects.
 * User code previously added there should be copied in the new user sections before
 * the section contents can be deleted.
 */
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */
#endif

/* USER CODE BEGIN DECL */

/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include "ff_gen_drv.h"
#include "stm32f1xx.h"
#include "diskio.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;
#define SD_CS_LOW()			HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define SD_CS_HIGH()		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)

#define CMD0		0
#define CMD1		1
#define CMD8		8
#define CMD16		16
#define CMD17		17
#define CMD24		24
#define CMD55		55
#define CMD58		58
#define ACMD41	41

extern SPI_HandleTypeDef hspi1;

static uint8_t SD_Type = 0; //unknown


static uint8_t SPI_TxRx(uint8_t data){
	uint8_t rx;
	HAL_SPI_TransmitReceive(&hspi1, &data, &rx, 1, HAL_MAX_DELAY);
	return rx;
}


static void SPI_Clock(uint8_t n){
	while(n--) SPI_TxRx(0xFF);
}

//----Send commands
static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg, uint8_t crc){
	uint8_t buf[6];
	uint8_t resp;
	uint16_t	retry = 0xFFFF; 
	
	buf[0] = 0x40 | cmd;
	buf[1] = (arg >> 24) & (0xFF);
	buf[2] = (arg >> 16) & (0xFF);
	buf[3] = (arg >> 8) & (0xFF);
	buf[4] = (arg >> 0) & (0xFF);
	buf[5] = crc;
	
	SD_CS_LOW();
	
	SPI_Clock(1);
	
	HAL_SPI_Transmit(&hspi1, buf, 6, HAL_MAX_DELAY);
	
	//Wait resp
	do{
		HAL_SPI_Receive(&hspi1, &resp, 1, HAL_MAX_DELAY);
	}while((resp & 0x80) && --retry);
	
	return resp;
}

static uint8_t SD_ReadBlock(uint8_t *buff, uint32_t add){
	uint8_t token;
	uint16_t cnt = 0xFFFF;
	if(SD_Type != 3){
		add = add << 9;
	}
	
	if(SD_SendCmd(CMD17, add, 0xFF) != 0x00){
		SD_CS_HIGH();
		return 1;
	}
	
	do{
		token = SPI_TxRx(0xFF);
	}while(token == 0xFF && --cnt);
	
	if(token != 0xFE){
		SD_CS_HIGH();
		return 2;
	}
	for(uint16_t i = 0 ; i < 512 ; i++){
			buff[i] = SPI_TxRx(0xFF);
	}
	
	SPI_TxRx(0xFF);
	SPI_TxRx(0xFF);
	SD_CS_HIGH();
	SPI_TxRx(0xFF);
	
	return 0;
}
static uint8_t SD_WriteBlock(const uint8_t *buff, uint32_t add){
	uint8_t resp;
	if(SD_Type != 3){
		add = add << 9;
	}
	
	if(SD_SendCmd(CMD24, add, 0xFF) != 0x00){
		SD_CS_HIGH();
		return 1;
	}
	
	SPI_TxRx(0xFE); //Token
	for(uint16_t i = 0 ; i < 512 ; i++){
			SPI_TxRx(buff[i]);
	}
	SPI_TxRx(0xFF);
	SPI_TxRx(0xFF);
	resp = SPI_TxRx(0xFF);
    if((resp & 0x1F) != 0x05) { SD_CS_HIGH(); return 2; }
	while(SPI_TxRx(0xFF) == 0x00);
	SD_CS_HIGH();
	SPI_TxRx(0xFF);
	return 0;
}

int SD_GetCSD(uint8_t *csd)
{
    uint8_t resp;
    uint16_t retry = 0xFFFF;

    resp = SD_SendCmd(9, 0, 0xFF);  // CMD9: SEND_CSD
    if (resp != 0x00) {
        SD_CS_HIGH();
        return -1;
    }

    // d?i token 0xFE
    while (--retry && (SPI_TxRx(0xFF) != 0xFE));

    if (!retry) {
        SD_CS_HIGH();
        return -2;
    }

    for (int i = 0; i < 16; i++) {
        csd[i] = SPI_TxRx(0xFF);
    }

    SPI_TxRx(0xFF); // CRC hi
    SPI_TxRx(0xFF); // CRC lo
    SD_CS_HIGH();
    SPI_TxRx(0xFF);

    return 0;
}

/* USER CODE END DECL */

/* Private function prototypes -----------------------------------------------*/
DSTATUS USER_initialize (BYTE pdrv);
DSTATUS USER_status (BYTE pdrv);
DRESULT USER_read (BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
  DRESULT USER_write (BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
#endif /* _USE_WRITE == 1 */
#if _USE_IOCTL == 1
  DRESULT USER_ioctl (BYTE pdrv, BYTE cmd, void *buff);
#endif /* _USE_IOCTL == 1 */

Diskio_drvTypeDef  USER_Driver =
{
  USER_initialize,
  USER_status,
  USER_read,
#if  _USE_WRITE
  USER_write,
#endif  /* _USE_WRITE == 1 */
#if  _USE_IOCTL == 1
  USER_ioctl,
#endif /* _USE_IOCTL == 1 */
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes a Drive
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_initialize (
	BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
  /* USER CODE BEGIN INIT */
	uint8_t resp;
	uint16_t retry;
	

	SD_CS_HIGH();
	SPI_Clock(10); //74
	// CMD0
	retry = 0x1FF;
	do{
		resp = SD_SendCmd(CMD0, 0, 0x95);
		SD_CS_HIGH();
		SPI_TxRx(0xFF);
	}while(resp != 0x01 && --retry);
	
	if(retry == 0){
		return STA_NOINIT;
	}
	// CMD8
	resp = SD_SendCmd(CMD8, 0x1AA, 0x87);
	
	if(resp == 0x01){
	  //V2
		uint8_t rs[4];
		for(uint8_t i = 0 ; i < 4 ; i++){
			rs[i] = SPI_TxRx(0xFF);
		}
		
		retry = 0xFFFF;
		do{
			SD_SendCmd(CMD55, 0, 0xFF);
			resp = SD_SendCmd(ACMD41, 0x40000000, 0xFF);
			SD_CS_HIGH();
			SPI_TxRx(0xFF);
		}while(resp != 0x00 && --retry);
		
		if(retry == 0){
			return STA_NOINIT;
		}
		
		resp = SD_SendCmd(CMD58, 0, 0xFF);
		uint8_t ocr[4];
		for(uint8_t i = 0 ; i < 4 ; i++){
			ocr[i] = SPI_TxRx(0xFF);
		}
		SD_Type = (ocr[0] & 0x40) ? 3 : 2;// SDHC - SDSC
		SD_CS_HIGH();
		SPI_TxRx(0xFF);
	}else{
	  //V1
		SD_Type = 1;
	}
	
	SD_SendCmd(CMD16, 512, 0xFF);
	SD_CS_HIGH();
	SPI_TxRx(0xFF);
	
	return 0;
  /* USER CODE END INIT */
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS USER_status (
	BYTE pdrv       /* Physical drive number to identify the drive */
)
{
  /* USER CODE BEGIN STATUS */
	return 0;
  /* USER CODE END STATUS */
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT USER_read (
	BYTE pdrv,      /* Physical drive nmuber to identify the drive */
	BYTE *buff,     /* Data buffer to store read data */
	DWORD sector,   /* Sector address in LBA */
	UINT count      /* Number of sectors to read */
)
{
  /* USER CODE BEGIN READ */
		for(UINT i = 0 ; i < count ;i++){
			if(SD_ReadBlock(buff+(i*512), sector + i)){
				return RES_ERROR;
			}
		}
    return RES_OK;
  /* USER CODE END READ */
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  *buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
#if _USE_WRITE == 1
DRESULT USER_write (
	BYTE pdrv,          /* Physical drive nmuber to identify the drive */
	const BYTE *buff,   /* Data to be written */
	DWORD sector,       /* Sector address in LBA */
	UINT count          /* Number of sectors to write */
)
{
  /* USER CODE BEGIN WRITE */
  /* USER CODE HERE */
		for(UINT i = 0 ; i < count ;i++){
			if(SD_WriteBlock(buff+(i*512), sector + i)){
				return RES_ERROR;
			}
		}
    return RES_OK;
  /* USER CODE END WRITE */
}
#endif /* _USE_WRITE == 1 */

/**
  * @brief  I/O control operation
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  *buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
#if _USE_IOCTL == 1
DRESULT USER_ioctl (
	BYTE pdrv,      /* Physical drive nmuber (0..) */
	BYTE cmd,       /* Control code */
	void *buff      /* Buffer to send/receive control data */
)
{
  /* USER CODE BEGIN IOCTL */
	switch(cmd){
		case CTRL_SYNC: return RES_OK;
		case GET_SECTOR_SIZE: *(WORD*)buff = 512; return RES_OK;
		case GET_BLOCK_SIZE: *(WORD*)buff = 1; return RES_OK;
		case GET_SECTOR_COUNT: *(WORD*)buff = 32768; return RES_OK;
	}
	return RES_PARERR;
  /* USER CODE END IOCTL */
}
#endif /* _USE_IOCTL == 1 */

