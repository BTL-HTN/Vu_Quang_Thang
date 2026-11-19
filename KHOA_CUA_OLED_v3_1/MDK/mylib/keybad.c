#include "keybad.h"

// Bang ma phim 4x4 : keys[hang][cot]
static const char keys[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// Delay ngan cho dien ap on dinh sau khi doi cot (khoang vai us)
static void keypad_delay(void)
{
    volatile uint32_t i;
    for (i = 0; i < 300; i++) {}
}

// Dua ca 4 cot len muc 1:  C1 = PA8, C2 = PB15, C3 = PB14, C4 = PB13
static void col_all_high(void)
{
    GPIOA->BSRR = (1 << 8);
    GPIOB->BSRR = (1 << 15) | (1 << 14) | (1 << 13);
}

// Keo cot c xuong 0 (c = 0..3 ung C1..C4), cac cot con lai = 1
static void col_select(uint8_t c)
{
    col_all_high();
    if      (c == 0) GPIOA->BRR = (1 << 8);    // C1 = PA8
    else if (c == 1) GPIOB->BRR = (1 << 15);   // C2 = PB15
    else if (c == 2) GPIOB->BRR = (1 << 14);   // C3 = PB14
    else             GPIOB->BRR = (1 << 13);   // C4 = PB13
}

// Doc 4 hang -> 1 so 4 bit: bit0 = R1, bit1 = R2, bit2 = R3, bit3 = R4 (bit = 1 la dang bi nhan)
static uint8_t read_rows(void)
{
    uint8_t v = 0;
    if ((GPIOA->IDR & (1 << 12)) == 0) v |= 0x01;   // R1 = PA12
    if ((GPIOA->IDR & (1 << 11)) == 0) v |= 0x02;   // R2 = PA11
    if ((GPIOB->IDR & (1 <<  8)) == 0) v |= 0x04;   // R3 = PB8
    if ((GPIOB->IDR & (1 <<  9)) == 0) v |= 0x08;   // R4 = PB9
    return v;
}

void Keypad_Init(void)
{
    // 1. Bat clock GPIOA (bit2) va GPIOB (bit3)
    RCC->APB2ENR |= 0x04;
    RCC->APB2ENR |= 0x08;

    // 2. GPIOA->CRH (PA8..PA15), moi chan 4 bit (0x2 = output 2MHz, 0x8 = input pull-up/down)
    //    PA8  = C1 (output)  -> nibble0 = 0x2
    //    PA11 = R2 (input)   -> nibble3 = 0x8
    //    PA12 = R1 (input)   -> nibble4 = 0x8
    //    KHONG dong den PA9, PA10 (USART1 noi ESP32) va PA13, PA14 (SWD nap code)
    GPIOA->CRH &= ~0x000FF00F;
    GPIOA->CRH |=  0x00088002;
    GPIOA->BSRR = (1 << 11) | (1 << 12);          // ODR = 1 -> chon KEO LEN (pull-up)

    // 3. GPIOB->CRH (PB8..PB15)
    //    PB8  = R3 (input)  -> nibble0 = 0x8
    //    PB9  = R4 (input)  -> nibble1 = 0x8
    //    PB13 = C4, PB14 = C3, PB15 = C2 (output) -> nibble5,6,7 = 0x2
    GPIOB->CRH &= ~0xFFF000FF;
    GPIOB->CRH |=  0x22200088;
    GPIOB->BSRR = (1 << 8) | (1 << 9);            // PB8, PB9 keo len (pull-up)

    // 4. Ban dau tat ca cot o muc cao
    col_all_high();
}

char Keypad_Scan(void)
{
    uint8_t c, r, rows;
    char result = 0;

    for (c = 0; c < 4; c++) {
        col_select(c);                 // chi cot hien tai = 0
        keypad_delay();
        rows = read_rows();
        if (rows != 0) {
            for (r = 0; r < 4; r++) {
                if (rows & (1 << r)) {          // hang r dang bi nhan
                    result = keys[r][c];
                    break;
                }
            }
            break;
        }
    }
    col_all_high();
    return result;
}

char Keypad_GetKey(void)
{
    static char    last_raw = 0;      // gia tri quet lan truoc
    static uint8_t cnt = 0;           // so lan quet lien tiep cho cung 1 phim
    static uint8_t reported = 0;      // phim nay da bao chua
    char raw = Keypad_Scan();

    if (raw == 0) {                   // khong co phim -> reset trang thai
        last_raw = 0;
        cnt = 0;
        reported = 0;
        return 0;
    }
    if (raw == last_raw) {            // cung 1 phim nhu lan truoc
        if (cnt < 255) cnt++;
        if (cnt >= 3 && !reported) {  // on dinh 3 lan quet lien tiep (~40ms) moi tinh la bam that (loc nhieu)
            reported = 1;
            return raw;               // bao phim 1 lan duy nhat
        }
        return 0;                     // dang giu phim, khong lap lai
    }
    last_raw = raw;                   // phim moi, bat dau dem lai
    cnt = 1;
    reported = 0;
    return 0;
}
