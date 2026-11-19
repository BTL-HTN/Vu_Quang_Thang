#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

/* LED canh bao: PC14 -> dien tro 1k -> LED -> GND
   (PC14 chi chiu duoc ~3mA, toc do toi da 2MHz) */

void Led_Init(void);
void Led_On(void);
void Led_Off(void);
void Led_Toggle(void);

#endif
