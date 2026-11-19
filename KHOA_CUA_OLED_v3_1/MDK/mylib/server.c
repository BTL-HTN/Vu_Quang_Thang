#include "sever.h"

void Servo_Init(void)
{
    // 1. Bat clock: AFIO (bit0), GPIOB (bit3) tren APB2 ; TIM3 (bit1) tren APB1
    RCC->APB2ENR |= 0x01;
    RCC->APB2ENR |= 0x08;
    RCC->APB1ENR |= 0x02;

    // 2. PB1 nam trong GPIOB->CRL, nibble1 (bit 7:4)
    //    0xB = MODE 11 (output 50MHz) + CNF 10 (alternate function push-pull)
    GPIOB->CRL &= ~(0xF << 4);
    GPIOB->CRL |=  (0xB << 4);

    // 3. TIM3: 72MHz / 72 = 1MHz (moi nhip dem = 1us) ; 20000 nhip = 20ms = 50Hz
    TIM3->PSC = 72 - 1;
    TIM3->ARR = 20000 - 1;

    // 4. Kenh 4: PWM mode 1 (OC4M = 110) -> CCMR2 bit 14:12 = 0x6 -> 0x6000
    TIM3->CCMR2 &= ~0xFF00;
    TIM3->CCMR2 |=  0x6000;

    // 5. Bat dau: CC4E (CCER bit12) bat dau ra, CEN (CR1 bit0) bat bo dem
    TIM3->CCER |= (1 << 12);
    TIM3->CR1  |= 0x01;
}

void Servo_Write(uint8_t angle)
{
    // Servo tieu chuan: 0 do = 500us, 180 do = 2500us
    // pulse = 500 + angle * 2000 / 180  (don vi us, TIM3 dem 1us/nhip)
    TIM3->CCR4 = 500 + (angle * 2000 / 180);
}
