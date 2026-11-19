#ifndef __KEYPAD_H
#define __KEYPAD_H

#include "stm32f10x.h"

/* Ban phim ma tran 4x4, 8 chan GPIO (theo so do moi, co ESP32):
   HANG (INPUT pull-up) : R1 = PA12, R2 = PA11, R3 = PB8,  R4 = PB9
   COT  (OUTPUT)        : C1 = PA8,  C2 = PB15, C3 = PB14, C4 = PB13
   (PA9, PA10 nhuong cho USART1 noi ESP32; PA13, PA14 la SWD)

   Cach quet: lan luot keo 1 cot xuong 0, doc 4 hang. Hang nao = 0 la phim dang nhan.

   Ky tu phim:
        C1  C2  C3  C4
   R1:  1   2   3   A
   R2:  4   5   6   B
   R3:  7   8   9   C
   R4:  *   0   #   D                                           */

void Keypad_Init(void);
char Keypad_Scan(void);     // quet tuc thoi: tra ve phim dang giu (0 neu khong co phim)
char Keypad_GetKey(void);   // goi deu dan (~20ms/lan): tra ve phim 1 lan duy nhat moi lan nhan (co chong rung)

#endif
