#include "flash.h"

void flash_erase(uint32_t addr){
	FLASH_EraseInitTypeDef pErase;
	uint32_t num_page_err;
	pErase.PageAddress = addr;
	pErase.TypeErase = 0x00U;
	pErase.NbPages = 1;
	HAL_FLASHEx_Erase(&pErase, &num_page_err);
}



void flash_write(uint32_t addr, uint8_t *data, uint32_t length){
	uint32_t TypeProgram = FLASH_TYPEPROGRAM_HALFWORD;

	for(uint32_t i = 0; i + 1 < length; i += 2){
		HAL_FLASH_Program(TypeProgram, addr + i, (uint16_t)data[i] | ((uint16_t)data[i + 1] << 8));
	}

	if(length % 2 != 0){
		HAL_FLASH_Program(TypeProgram, addr + length - 1, (uint16_t)data[length - 1] | (0xFF << 8)); // 0xFF
	}
}

void flash_read(uint32_t addr, uint8_t* data, uint32_t length){
	for(uint32_t i=0; i<length; i+=2){
		volatile uint32_t* address = (volatile uint32_t*) (addr+i);
		uint16_t data_tem = *address;
		data[i] = (uint8_t) data_tem;
		data[i+1] = (uint8_t) (data_tem>>8);	
	}
}