#include "led.h"

void Led_Init(void)
{
    RCC->APB2ENR |= (1 << 4);          // bat clock GPIOC (IOPCEN = bit4)
    // PC14 nam trong thanh ghi CRH, nibble thu (14-8) = 6 -> bit 27:24
    GPIOC->CRH &= ~(0xF << 24);        // xoa cau hinh cu
    GPIOC->CRH |=  (0x2 << 24);        // MODE=10 (output 2MHz), CNF=00 (push-pull)
    GPIOC->BRR = (1 << 14);            // mac dinh tat LED
}

void Led_On(void)
{
    GPIOC->BSRR = (1 << 14);           // PC14 = 1 -> LED sang
}

void Led_Off(void)
{
    GPIOC->BRR = (1 << 14);            // PC14 = 0 -> LED tat
}

void Led_Toggle(void)
{
    GPIOC->ODR ^= (1 << 14);           // dao trang thai
}
