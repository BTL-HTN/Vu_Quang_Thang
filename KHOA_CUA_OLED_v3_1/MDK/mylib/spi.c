#include "spi.h"

#define SPI_TIMEOUT 20000   // so vong cho toi da, tranh treo he thong neu loi phan cung

void Spi1_Init(void)
{
    // 1. Bat clock: AFIO (bit0) + GPIOA (bit2) + SPI1 (bit12)
    RCC->APB2ENR |= 0x01;      // AFIOEN
    RCC->APB2ENR |= (1 << 2);  // IOPAEN
    RCC->APB2ENR |= (1 << 12); // SPI1EN

    // 2. Cau hinh chan PA4..PA7 (thanh ghi CRL, moi chan 4 bit)
    //    PA4 (SS)   : Output push-pull 50MHz  -> 0x3
    //    PA5 (SCK)  : AF push-pull 50MHz      -> 0xB
    //    PA6 (MISO) : Input floating          -> 0x4
    //    PA7 (MOSI) : AF push-pull 50MHz      -> 0xB
    GPIOA->CRL &= ~0xFFFF0000;    // xoa cau hinh cu cua PA4..PA7 (khong dong cac chan khac)
    GPIOA->CRL |=  0xB4B30000;    // nibble7=B, nibble6=4, nibble5=B, nibble4=3

    // 3. SS mac dinh muc cao (chua chon RC522)
    GPIOA->BSRR = (1 << 4);

    // 4. Cau hinh SPI1
    //    bit2  MSTR = 1 : master
    //    bit5:3 BR = 100: chia 32 -> 72MHz/32 = 2.25MHz (RC522 toi da 10MHz)
    //    bit8  SSI = 1, bit9 SSM = 1 : quan ly NSS bang phan mem
    //    CPOL = 0, CPHA = 0 (mode 0), DFF = 0 (8 bit), LSBFIRST = 0 (MSB truoc)
    SPI1->CR1 = 0x0300 | 0x0004 | (0x4 << 3);   // = 0x0324
    SPI1->CR2 = 0x0000;
    SPI1->CR1 |= (1 << 6);                      // SPE = 1: bat SPI1
}

uint8_t Spi1_Transfer(uint8_t data)
{
    uint32_t t;

    t = SPI_TIMEOUT;
    while (((SPI1->SR & 0x02) == 0) && --t) {}   // cho TXE = 1 (bo dem gui trong)
    SPI1->DR = data;

    t = SPI_TIMEOUT;
    while (((SPI1->SR & 0x01) == 0) && --t) {}   // cho RXNE = 1 (da nhan xong 1 byte)
    return (uint8_t)SPI1->DR;
}

void Spi1_Select(void)
{
    GPIOA->BRR = (1 << 4);     // SS = 0
}

void Spi1_Deselect(void)
{
    uint32_t t;
    t = SPI_TIMEOUT;
    while ((SPI1->SR & 0x80) && --t) {}   // cho BSY = 0 (da gui het) roi moi nha SS
    GPIOA->BSRR = (1 << 4);    // SS = 1
}
