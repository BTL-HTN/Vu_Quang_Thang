
#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"
#include <stdint.h>

typedef enum
{
    BUZZER_ACTIVE_HIGH = 0,
    BUZZER_ACTIVE_LOW
} Buzzer_Polarity_t;

typedef struct
{
    GPIO_TypeDef *Port;
    uint16_t Pin;
    Buzzer_Polarity_t Polarity;
} Buzzer_Config_t;

void Buzzer_Init(Buzzer_Config_t *config);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Toggle(void);
void Buzzer_Beep(uint32_t duration_ms);
void Buzzer_BeepTimes(uint8_t times,
                      uint32_t on_ms,
                      uint32_t off_ms);

#endif
