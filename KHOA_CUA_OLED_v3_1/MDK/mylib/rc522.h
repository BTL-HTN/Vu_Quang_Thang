#ifndef __RC522_H
#define __RC522_H

#include "stm32f10x.h"

/* Thu vien RC522 (MFRC522) viet rieng, dung SPI1 (xem spi.h)
   RST = PB0 */

#define RC522_OK     0   // thanh cong
#define RC522_NOTAG  1   // khong co the trong vung doc
#define RC522_ERR    2   // loi giao tiep / loi du lieu

uint8_t RC522_Init(void);              // tra ve 1 neu tim thay chip RC522, 0 neu khong
uint8_t RC522_ReadVersion(void);       // doc thanh ghi VersionReg (thuong 0x91 hoac 0x92)
uint8_t RC522_Request(void);           // hoi xem co the o gan khong (REQA)
uint8_t RC522_Anticoll(uint8_t *uid);  // doc UID 4 byte (uid[4])
void    RC522_Halt(void);              // cho the vao trang thai HALT (khong doc lai den khi rut the ra)

#endif
