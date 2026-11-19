#ifndef __SPI_H
#define __SPI_H

#include "stm32f10x.h"

/* SPI1 dung cho RC522 (ghi truc tiep thanh ghi)
   PA4 = SS (NSS bang phan mem, GPIO output)
   PA5 = SCK, PA6 = MISO, PA7 = MOSI */

void Spi1_Init(void);                 // cau hinh chan + SPI1 master mode 0
uint8_t Spi1_Transfer(uint8_t data);  // gui 1 byte, tra ve byte nhan duoc
void Spi1_Select(void);               // keo SS xuong 0 (bat dau giao tiep)
void Spi1_Deselect(void);             // keo SS len 1 (ket thuc giao tiep)

#endif
