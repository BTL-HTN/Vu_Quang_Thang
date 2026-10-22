#include "mPrint.h"
#include <stdarg.h>
#include <stdio.h>

static char buff[128]; 
static USART_TypeDef *DEBUG_USART = USART1; // M?c d?nh dùng USART1

void Config_Uart_Print(uint8_t uart) {
    RCC->APB2ENR |= (1 << 0); // Enable AFIO clock

    if (uart == 1) {
        RCC->APB2ENR |= (1 << 14) | (1 << 2); // USART1 + GPIOA
        DEBUG_USART = USART1;
        DEBUG_USART->BRR = 0x1D4C; // 9600 @ 72MHz
        GPIOA->CRH &= ~(0xF << 4); GPIOA->CRH |= (0xB << 4); // TX PA9
        GPIOA->CRH &= ~(0xF << 8); GPIOA->CRH |= (0x4 << 8); // RX PA10
    }
    else if (uart == 2) {
        RCC->APB1ENR |= (1 << 17); RCC->APB2ENR |= (1 << 2); // USART2 + GPIOA
        DEBUG_USART = USART2;
        DEBUG_USART->BRR = 0x0EA6; // 9600 @ 36MHz
        GPIOA->CRL &= ~(0xF << 8); GPIOA->CRL |= (0xB << 8); // TX PA2
        GPIOA->CRL &= ~(0xF << 12); GPIOA->CRL |= (0x4 << 12); // RX PA3
    }
    else if (uart == 3) {
        RCC->APB1ENR |= (1 << 18); RCC->APB2ENR |= (1 << 3); // USART3 + GPIOB
        DEBUG_USART = USART3;
        DEBUG_USART->BRR = 0x0EA6; // 9600 @ 36MHz
        GPIOB->CRH &= ~(0xF << 8); GPIOB->CRH |= (0xB << 8); // TX PB10
        GPIOB->CRH &= ~(0xF << 12); GPIOB->CRH |= (0x4 << 12); // RX PB11
    }
    
    DEBUG_USART->CR1 |= (1 << 3) | (1 << 13); // TE, UE
}

static void SendChar(char c) {
    // Ch? cho d?n khi thanh ghi d? li?u tr?ng (TXE = 1)
    while (!(DEBUG_USART->SR & (1 << 7))); 
    DEBUG_USART->DR = c;
}

void mPrint(const char* format, ...) {
    va_list args;
    int len; // KHAI BÁO BI?N ? TRÊN CÙNG
    int i;   // KHAI BÁO BI?N ? TRÊN CÙNG

    va_start(args, format);
    
    // Sau khi khai báo xong h?t m?i d?n dòng l?nh th?c thi này
    len = vsnprintf(buff, sizeof(buff), format, args);
    
    va_end(args);

    if (len > 0) {
        for (i = 0; i < len; i++) {
            SendChar(buff[i]);
        }
    }
}
// NH? NH?N ENTER T?O 1 DÒNG TR?NG ? ÐÂY Ð? H?T WARNING #1-D