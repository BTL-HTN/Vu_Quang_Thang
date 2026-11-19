#ifndef __STORE_H
#define __STORE_H

#include "stm32f10x.h"

/* Luu danh sach the RFID + 2 mat khau vao Flash noi cua STM32F103C8
   Dung trang cuoi cung: 0x0800FC00 (1KB) - khong bi mat khi mat dien */

#define STORE_MAX_CARDS  20    // toi da 20 the
#define STORE_PW_MAX     8     // mat khau toi da 8 so
#define STORE_PW_MIN     4     // mat khau toi thieu 4 so

#define PW_USER   0
#define PW_ADMIN  1

#define STORE_ADD_OK      0
#define STORE_ADD_EXIST   1
#define STORE_ADD_FULL    2

void    Store_Load(void);                       // doc tu Flash (neu trong thi dung gia tri mac dinh)
uint8_t Store_Save(void);                       // ghi RAM -> Flash, tra ve 1 neu ghi dung
int     Store_FindCard(const uint8_t *uid);     // tra ve vi tri the (>=0) hoac -1 neu khong co
uint8_t Store_AddCard(const uint8_t *uid);      // STORE_ADD_OK / EXIST / FULL
uint8_t Store_DelCard(const uint8_t *uid);      // 1 = da xoa, 0 = khong tim thay
uint8_t Store_CountCards(void);
uint8_t Store_CheckPw(uint8_t who, const char *pw);   // 1 = dung, 0 = sai
void    Store_SetPw(uint8_t who, const char *pw);

#endif
