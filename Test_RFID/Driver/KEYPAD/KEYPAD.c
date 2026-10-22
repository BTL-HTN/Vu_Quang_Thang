
#include "KEYPAD.h"

static Keypad4x4_Config_t *keypad;

static const char keymap[4][4] =
    {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}};

static void Delay_ms(uint32_t ms)
{
    volatile uint32_t i;

    while (ms--)
    {
        for (i = 0; i < 8000; i++)
            __NOP();
    }
}

static void EnableGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (port == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (port == GPIOC)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
}

static void SetRow(uint8_t row, BitAction state)
{
    GPIO_WriteBit(keypad->RowPort[row],
                  keypad->RowPin[row],
                  state);
}

static uint8_t ReadColumn(uint8_t col)
{
    return GPIO_ReadInputDataBit(keypad->ColPort[col],
                                 keypad->ColPin[col]) == Bit_RESET;
}

static void AllRowsHigh(void)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
        SetRow(i, Bit_SET);
}

void Keypad4x4_Init(Keypad4x4_Config_t *config)
{
    GPIO_InitTypeDef gpio;
    uint8_t i;

    if (config == 0)
        return;

    keypad = config;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    for (i = 0; i < 4; i++)
    {
        EnableGPIOClock(keypad->RowPort[i]);
        EnableGPIOClock(keypad->ColPort[i]);
    }

    for (i = 0; i < 4; i++)
    {
        gpio.GPIO_Pin = keypad->RowPin[i];
        gpio.GPIO_Mode = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(keypad->RowPort[i], &gpio);

        gpio.GPIO_Pin = keypad->ColPin[i];
        gpio.GPIO_Mode = GPIO_Mode_IPU;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(keypad->ColPort[i], &gpio);
    }

    AllRowsHigh();
}

static char ScanRaw(void)
{
    uint8_t row, col;

    for (row = 0; row < 4; row++)
    {
        AllRowsHigh();
        SetRow(row, Bit_RESET);
        Delay_ms(1);

        for (col = 0; col < 4; col++)
        {
            if (ReadColumn(col))
            {
                AllRowsHigh();
                return keymap[row][col];
            }
        }
    }

    AllRowsHigh();
    return KEYPAD_NO_KEY;
}

char Keypad4x4_GetKey(void)
{
    char key;
    uint16_t debounce;

    if (keypad == 0)
        return KEYPAD_NO_KEY;

    key = ScanRaw();

    if (key == KEYPAD_NO_KEY)
        return KEYPAD_NO_KEY;

    debounce = keypad->Debounce_ms;

    if (debounce == 0)
        debounce = 20;

    Delay_ms(debounce);

    if (ScanRaw() != key)
        return KEYPAD_NO_KEY;

    while (ScanRaw() != KEYPAD_NO_KEY)
        Delay_ms(5);

    Delay_ms(debounce);

    return key;
}

char Keypad4x4_WaitKey(void)
{
    char key;

    do
    {
        key = Keypad4x4_GetKey();
    } while (key == KEYPAD_NO_KEY);

    return key;
}
