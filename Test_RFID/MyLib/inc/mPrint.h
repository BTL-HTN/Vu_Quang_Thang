#ifndef __MPRINT_H
#define __MPRINT_H

#include "stm32f10x.h"
#include <stdio.h>
#include <stdarg.h>

// Khai báo các hàm d? file khác có th? g?i du?c
void Config_Uart_Print(uint8_t uart);
void mPrint(const char* format, ...);

#endif