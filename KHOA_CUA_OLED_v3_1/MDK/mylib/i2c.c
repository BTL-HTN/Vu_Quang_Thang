#include "i2c.h"

/* ---------------------------------------------------------------
   GHI CHU SUA DOI so voi ban cu:
   - Cac ham van giu nguyen ten va tham so (i2c_init, i2c_start, i2c_add,
     i2c_data, i2c_stop, i2c_write) nen LCD.C khong can sua.
   - Them "cho co gioi han" (I2C_TIMEOUT): neu LCD cam sai/dia chi sai thi
     ham se bo qua thay vi treo vong lap vo han lam dung ca RTOS.
   - i2c_start: doi bus ranh (BUSY=0) truoc khi phat START.
   - i2c_stop : doi gui xong byte cuoi (BTF) roi moi STOP, va doi STOP hoan tat.
   --------------------------------------------------------------- */

#define I2C_TIMEOUT 60000    // so vong cho toi da cho moi buoc

static uint8_t i2c_fail[3];  // co loi cua tung bus: [1] = I2C1, [2] = I2C2

// Delay ngan (khoang vai us) cho cac xung SCL khi giai phong bus
static void i2c_dly(void)
{
    volatile uint32_t i;
    for (i = 0; i < 100; i++) {}
}

/* GIAI PHONG BUS I2C: neu man hinh dang giu SDA o muc thap (vi chip bi reset giua luc truyen,
   hoac nhieu dien tu) thi bus bi ket va I2C khong the START.
   Cach go: tam thoi dung SCL/SDA nhu GPIO open-drain, phat toi da 9 xung SCL cho man hinh nha SDA,
   roi tao dieu kien STOP.  I2C1: SCL = PB6, SDA = PB7 ; I2C2: SCL = PB10, SDA = PB11 */
static void i2c_bus_recover(char i2c)
{
    volatile uint32_t *cr;
    uint8_t scl, sda, sh_scl, sh_sda, n;

    if (i2c == 1) { cr = &GPIOB->CRL; scl = 6;  sda = 7;  sh_scl = 24; sh_sda = 28; }
    else          { cr = &GPIOB->CRH; scl = 10; sda = 11; sh_scl = 8;  sh_sda = 12; }

    // SCL, SDA = output open-drain 2MHz (MODE = 10, CNF = 01 -> 0x6), dua len muc 1 (nha bus)
    *cr &= ~((0xF << sh_scl) | (0xF << sh_sda));
    *cr |=  ((0x6 << sh_scl) | (0x6 << sh_sda));
    GPIOB->BSRR = (1 << scl) | (1 << sda);
    i2c_dly();

    // Neu SDA dang bi giu thap: phat xung SCL cho toi khi SDA duoc nha (toi da 9 xung)
    for (n = 0; n < 9 && ((GPIOB->IDR & (1 << sda)) == 0); n++) {
        GPIOB->BRR  = (1 << scl);  i2c_dly();
        GPIOB->BSRR = (1 << scl);  i2c_dly();
    }

    // Tao dieu kien STOP: SDA keo len trong luc SCL = 1
    GPIOB->BRR  = (1 << sda);  i2c_dly();
    GPIOB->BSRR = (1 << scl);  i2c_dly();
    GPIOB->BSRR = (1 << sda);  i2c_dly();
}

static I2C_TypeDef *i2c_sel(char i2c)
{
    return (i2c == 1) ? I2C1 : I2C2;
}

void i2c_init(char i2c, unsigned short speed_mode)  // cau hinh cho i2c
{
    RCC->APB2ENR |= 1;      // bat AFIO

    if (i2c == 1)
    {
        RCC->APB1ENR |= 0x200000;     // cap clock cho I2C1 (bit21)
        RCC->APB2ENR |= (1 << 3);     // bat clock GPIOB (IOPBEN)

        I2C1->CR1 &= ~1;              // tat I2C1 truoc khi giai phong bus
        i2c_bus_recover(1);           // nha bus neu man hinh dang giu SDA

        // PB6 = SCL : AF open-drain 50MHz (nibble = 0xF)
        GPIOB->CRL &= ~(0xF << 24);
        GPIOB->CRL |=  (0xF << 24);

        // PB7 = SDA : AF open-drain 50MHz (nibble = 0xF)
        GPIOB->CRL &= ~(0xF << 28);
        GPIOB->CRL |=  (0xF << 28);

        I2C1->CR1 |= 0x8000;          // SWRST = 1 de lam moi
        I2C1->CR1 &= ~0x8000;         // SWRST = 0
        I2C1->CR2 = 36;               // tan so APB1 = 36MHz (chip chay 72MHz)
        I2C1->CCR = 180;              // toc do 100kHz
        I2C1->TRISE = 37;
        I2C1->CR1 |= 1;               // PE = 1: bat I2C1
    }
    else if (i2c == 2)
    {
        RCC->APB1ENR |= 0x400000;
        RCC->APB2ENR |= (1 << 3);

        I2C2->CR1 &= ~1;
        i2c_bus_recover(2);

        // PB10 = SCL
        GPIOB->CRH &= ~(0xF << 8);
        GPIOB->CRH |=  (0xF << 8);

        // PB11 = SDA
        GPIOB->CRH &= ~(0xF << 12);
        GPIOB->CRH |=  (0xF << 12);

        I2C2->CR1 |= 0x8000;
        I2C2->CR1 &= ~0x8000;
        I2C2->CR2 = 0x8;
        I2C2->CCR = speed_mode;
        I2C2->TRISE = 0x9;
        I2C2->CR1 |= 1;
    }
}

// Buoc START
void i2c_start(char i2c)
{
    I2C_TypeDef *p = i2c_sel(i2c);
    uint32_t t;

    i2c_fail[(uint8_t)i2c & 3] = 0;           // xoa co loi cho giao dich moi
    p->SR1 &= ~0xDF00;                        // xoa cac co loi cu: BERR, ARLO, AF, OVR, PECERR, TIMEOUT, SMBALERT

    t = I2C_TIMEOUT;
    while ((p->SR2 & 0x02) && --t) {}         // cho bus ranh (BUSY = SR2 bit1 = 0)
    if (t == 0) i2c_init(i2c, 0x2d);          // bus bi ket -> giai phong bus va cau hinh lai

    p->CR1 |= 0x100;                          // START = 1 (CR1 bit8)
    t = I2C_TIMEOUT;
    while (!(p->SR1 & 0x01) && --t) {}        // cho SB = 1 (da phat START)
    if (t == 0) {
        i2c_fail[(uint8_t)i2c & 3] = 1;       // khong START duoc -> bao loi
        i2c_init(i2c, 0x2d);                  // reset I2C + giai phong bus de lan sau chay lai
    }
}

// Gui dia chi + bit R/W
void i2c_add(char i2c, char address, char RW)
{
    I2C_TypeDef *p = i2c_sel(i2c);
    volatile int tmp;
    uint32_t t;

    if (i2c_fail[(uint8_t)i2c & 3]) return;

    p->DR = (uint8_t)(address | RW);          // nap dia chi vao DR

    t = I2C_TIMEOUT;
    // cho ADDR = 1 (bit1) hoac AF = 1 (bit10, thiet bi khong tra loi ACK)
    while (((p->SR1 & 0x0002) == 0) && ((p->SR1 & 0x0400) == 0) && --t) {}

    if ((p->SR1 & 0x0002) != 0)
    {
        tmp = p->SR1;                         // xoa ADDR: doc SR1 roi doc SR2
        tmp = p->SR2;
    }
    else
    {
        p->SR1 &= ~0x0400;                    // xoa co AF
        p->CR1 |= 0x200;                      // STOP = 1
        i2c_fail[(uint8_t)i2c & 3] = 1;       // danh dau loi -> cac ham sau bo qua
    }
    (void)tmp;
}

// Gui 1 byte du lieu
void i2c_data(char i2c, char data)
{
    I2C_TypeDef *p = i2c_sel(i2c);
    uint32_t t;

    if (i2c_fail[(uint8_t)i2c & 3]) return;

    t = I2C_TIMEOUT;
    while (((p->SR1 & 0x80) == 0) && --t) {}  // cho TXE = 1 (DR trong) roi moi gui
    if (t == 0) { i2c_fail[(uint8_t)i2c & 3] = 1; return; }

    p->DR = (uint8_t)data;
    t = I2C_TIMEOUT;
    while (((p->SR1 & 0x80) == 0) && (p->SR1 & 0x0400) == 0 && --t) {}  // cho TXE = 1 lai hoac AF (bi NACK)
    if (t == 0 || (p->SR1 & 0x0400)) {
        p->SR1 &= ~0x0400;                    // xoa co AF
        p->CR1 |= 0x200;                      // STOP = 1
        i2c_fail[(uint8_t)i2c & 3] = 1;       // danh dau loi -> cac ham sau bo qua
    }
}

// Buoc STOP
void i2c_stop(char i2c)
{
    I2C_TypeDef *p = i2c_sel(i2c);
    uint32_t t;

    if (i2c_fail[(uint8_t)i2c & 3]) return;   // da STOP khi loi dia chi

    t = I2C_TIMEOUT;
    while (((p->SR1 & 0x04) == 0) && --t) {}  // cho BTF = 1 (byte cuoi da gui xong han)
    p->CR1 |= 0x200;                          // STOP = 1 (CR1 bit9)
    t = I2C_TIMEOUT;
    while ((p->CR1 & 0x200) && --t) {}        // cho STOP hoan tat (phan cung tu xoa bit)
}

// Gui ca chuoi (tong cua cac ham tren)
void i2c_write(char i2c, char address, char data[])
{
    int i = 0;

    i2c_start(i2c);
    i2c_add(i2c, address, 0);

    while (data[i] != '\0')
    {
        i2c_data(i2c, data[i]);
        i++;
    }
    i2c_stop(i2c);
}

// Tra ve 1 neu giao dich I2C vua roi bi loi (thiet bi khong tra loi ACK hoac bus ket)
uint8_t i2c_failed(char i2c)
{
    return i2c_fail[(uint8_t)i2c & 3];
}
