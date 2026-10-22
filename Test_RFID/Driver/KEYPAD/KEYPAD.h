
#ifndef __KEYPAD_H
#define __KEYPAD_H

#include "stm32f10x.h"
#include <stdint.h>

#define KEYPAD_NO_KEY '\0'

typedef struct
{
    GPIO_TypeDef *RowPort[4];
    uint16_t RowPin[4];

    GPIO_TypeDef *ColPort[4];
    uint16_t ColPin[4];

    uint16_t Debounce_ms;
} Keypad4x4_Config_t;

void Keypad4x4_Init(Keypad4x4_Config_t *config);
char Keypad4x4_GetKey(void);
char Keypad4x4_WaitKey(void);

#endif
