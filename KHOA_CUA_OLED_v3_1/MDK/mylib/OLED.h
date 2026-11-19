#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

/* OLED 1.3 inch I2C (128x64, chip SSD1306/SH1106), dia chi I2C 0x78 (0x3C dich trai 1 bit)
   Noi day: GND, VCC (3.3V hoac 5V), SCK -> SCL (PB6), SDA -> SDA (PB7) khi dung I2C1

   Man hinh chia 8 "page" (0..7), moi page cao 8 diem; 128 cot (0..127).
   Chu 5x7 diem + 1 cot trong = 6 diem moi ky tu -> toi da 21 ky tu / dong.

   OLED_COL_OFFSET: chip SH1106 (hay gap tren OLED 1.3") co RAM 132 cot, man hinh hien 128 cot
   nam giua nen phai cong them 2 cot. Neu la chip SSD1306 that thi doi thanh 0.            */
#define OLED_COL_OFFSET  2

void oled_init(char i2c);                                          // khoi tao OLED (i2c = 1 hoac 2)
void oled_blank(char i2c);                                         // xoa sach man hinh
void oled_msg(char i2c, uint8_t Ypos, uint8_t Xpos, char str[]);   // in chuoi: Ypos = page 0..7, Xpos = cot 0..127

#endif
