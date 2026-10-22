
#include "BUZZER.h"

static Buzzer_Config_t *buzzer;

static void EnableGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (port == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (port == GPIOC)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
}

static void Delay_ms(uint32_t ms)
{
    volatile uint32_t i;

    while (ms--)
    {
        for (i = 0; i < 8000; i++)
            __NOP();
    }
}

void Buzzer_Init(Buzzer_Config_t *config)
{
    GPIO_InitTypeDef gpio;

    if (config == 0)
        return;

    buzzer = config;

    EnableGPIOClock(buzzer->Port);

    gpio.GPIO_Pin = buzzer->Pin;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    GPIO_Init(buzzer->Port, &gpio);

    Buzzer_Off();
}

void Buzzer_On(void)
{
    if (buzzer == 0)
        return;

    if (buzzer->Polarity == BUZZER_ACTIVE_HIGH)
        GPIO_SetBits(buzzer->Port, buzzer->Pin);
    else
        GPIO_ResetBits(buzzer->Port, buzzer->Pin);
}

void Buzzer_Off(void)
{
    if (buzzer == 0)
        return;

    if (buzzer->Polarity == BUZZER_ACTIVE_HIGH)
        GPIO_ResetBits(buzzer->Port, buzzer->Pin);
    else
        GPIO_SetBits(buzzer->Port, buzzer->Pin);
}

void Buzzer_Toggle(void)
{
    if (buzzer == 0)
        return;

    GPIO_WriteBit(
        buzzer->Port,
        buzzer->Pin,
        (BitAction)(1U - GPIO_ReadOutputDataBit(
                             buzzer->Port, buzzer->Pin)));
}

void Buzzer_Beep(uint32_t duration_ms)
{
    Buzzer_On();
    Delay_ms(duration_ms);
    Buzzer_Off();
}

void Buzzer_BeepTimes(uint8_t times,
                      uint32_t on_ms,
                      uint32_t off_ms)
{
    uint8_t i;

    for (i = 0; i < times; i++)
    {
        Buzzer_Beep(on_ms);

        if (i < times - 1)
            Delay_ms(off_ms);
    }
}
