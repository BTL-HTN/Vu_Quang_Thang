#include "rc522.h"
#include "spi.h"

/* ---- Dia chi thanh ghi cua RC522 ---- */
#define REG_COMMAND     0x01
#define REG_COMIEN      0x02
#define REG_COMIRQ      0x04
#define REG_DIVIRQ      0x05
#define REG_ERROR       0x06
#define REG_FIFODATA    0x09
#define REG_FIFOLEVEL   0x0A
#define REG_CONTROL     0x0C
#define REG_BITFRAMING  0x0D
#define REG_MODE        0x11
#define REG_TXCONTROL   0x14
#define REG_TXASK       0x15
#define REG_CRCRES_H    0x21
#define REG_CRCRES_L    0x22
#define REG_RFCFG       0x26
#define REG_TMODE       0x2A
#define REG_TPRESCALER  0x2B
#define REG_TRELOAD_H   0x2C
#define REG_TRELOAD_L   0x2D
#define REG_VERSION     0x37

/* ---- Lenh cua RC522 ---- */
#define CMD_IDLE        0x00
#define CMD_CALCCRC     0x03
#define CMD_TRANSCEIVE  0x0C
#define CMD_SOFTRESET   0x0F

/* ---- Lenh gui cho the (chuan ISO14443A) ---- */
#define PICC_REQIDL     0x26
#define PICC_ANTICOLL   0x93
#define PICC_HALT       0x50

// Delay thoi gian bang vong lap (khong dung RTOS de thu vien doc lap)
static void rc_delay_ms(uint32_t ms)
{
    volatile uint32_t i;
    uint32_t n = ms * 8000;
    for (i = 0; i < n; i++) {}
}

// Ghi 1 byte vao thanh ghi
// Byte dia chi: bit7 = 0 (ghi), bit6..1 = dia chi, bit0 = 0
static void rc_write(uint8_t reg, uint8_t val)
{
    Spi1_Select();
    Spi1_Transfer((uint8_t)((reg << 1) & 0x7E));
    Spi1_Transfer(val);
    Spi1_Deselect();
}

// Doc 1 byte tu thanh ghi
// Byte dia chi: bit7 = 1 (doc), bit6..1 = dia chi, bit0 = 0
static uint8_t rc_read(uint8_t reg)
{
    uint8_t v;
    Spi1_Select();
    Spi1_Transfer((uint8_t)(((reg << 1) & 0x7E) | 0x80));
    v = Spi1_Transfer(0x00);
    Spi1_Deselect();
    return v;
}

static void rc_set_bit(uint8_t reg, uint8_t mask)
{
    rc_write(reg, (uint8_t)(rc_read(reg) | mask));
}

static void rc_clear_bit(uint8_t reg, uint8_t mask)
{
    rc_write(reg, (uint8_t)(rc_read(reg) & (uint8_t)(~mask)));
}

// Tinh CRC_A cua RC522 (dung cho lenh HALT)
static void rc_calc_crc(uint8_t *data, uint8_t len, uint8_t *out)
{
    uint8_t i, n;

    rc_clear_bit(REG_DIVIRQ, 0x04);     // xoa co CRCIRq
    rc_set_bit(REG_FIFOLEVEL, 0x80);    // xoa FIFO
    for (i = 0; i < len; i++) rc_write(REG_FIFODATA, data[i]);
    rc_write(REG_COMMAND, CMD_CALCCRC);

    i = 0xFF;
    do {
        n = rc_read(REG_DIVIRQ);
        i--;
    } while ((i != 0) && !(n & 0x04));  // cho CRCIRq = 1

    out[0] = rc_read(REG_CRCRES_L);
    out[1] = rc_read(REG_CRCRES_H);
}

// Gui du lieu toi the va nhan tra loi (ham goc cua moi thao tac)
// backbits: so bit nhan duoc
static uint8_t rc_tocard(uint8_t *send, uint8_t sendlen, uint8_t *back, uint16_t *backbits)
{
    uint8_t status = RC522_ERR;
    uint8_t n, last_bits;
    uint16_t i;

    rc_write(REG_COMIEN, 0x77 | 0x80);   // cho phep cac ngat can theo doi
    rc_clear_bit(REG_COMIRQ, 0x80);      // xoa het co ngat
    rc_set_bit(REG_FIFOLEVEL, 0x80);     // xoa FIFO
    rc_write(REG_COMMAND, CMD_IDLE);

    for (i = 0; i < sendlen; i++) rc_write(REG_FIFODATA, send[i]);

    rc_write(REG_COMMAND, CMD_TRANSCEIVE);
    rc_set_bit(REG_BITFRAMING, 0x80);    // StartSend = 1: bat dau truyen

    // Cho: bit0 = TimerIRq (het gio), bit4 = IdleIRq, bit5 = RxIRq (nhan xong)
    i = 5000;
    do {
        n = rc_read(REG_COMIRQ);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & 0x30));

    rc_clear_bit(REG_BITFRAMING, 0x80);  // StartSend = 0

    if (i != 0) {
        // Kiem tra loi: bit0 ProtocolErr, bit1 ParityErr, bit3 CollErr, bit4 BufferOvfl -> mask 0x1B
        if ((rc_read(REG_ERROR) & 0x1B) == 0) {
            status = RC522_OK;
            if (n & 0x01) status = RC522_NOTAG;   // timer het gio => khong co the

            n = rc_read(REG_FIFOLEVEL);           // so byte trong FIFO
            last_bits = rc_read(REG_CONTROL) & 0x07;
            if (last_bits) *backbits = (uint16_t)((n - 1) * 8 + last_bits);
            else           *backbits = (uint16_t)(n * 8);

            if (n == 0)  n = 1;
            if (n > 16)  n = 16;
            for (i = 0; i < n; i++) back[i] = rc_read(REG_FIFODATA);
        }
    }
    return status;
}

uint8_t RC522_ReadVersion(void)
{
    return rc_read(REG_VERSION);
}

uint8_t RC522_Init(void)
{
    uint8_t ver;
    uint8_t tmp;

    // 1. Chan RST = PB0 (output push-pull 2MHz)
    RCC->APB2ENR |= (1 << 3);          // IOPBEN
    GPIOB->CRL &= ~0x0000000F;         // xoa cau hinh PB0
    GPIOB->CRL |=  0x00000002;         // MODE=10 (2MHz), CNF=00 (push-pull)

    // 2. Reset cung: RST xuong 0 roi len 1
    GPIOB->BRR  = (1 << 0);
    rc_delay_ms(5);
    GPIOB->BSRR = (1 << 0);
    rc_delay_ms(50);

    // 3. Reset mem
    rc_write(REG_COMMAND, CMD_SOFTRESET);
    rc_delay_ms(50);

    // 4. Cau hinh timer noi (timeout ~ 15ms cho moi lan hoi the)
    rc_write(REG_TMODE, 0x8D);         // TAuto = 1, TPrescaler cao = 0xD
    rc_write(REG_TPRESCALER, 0x3E);    // TPrescaler thap = 0x3E
    rc_write(REG_TRELOAD_L, 30);
    rc_write(REG_TRELOAD_H, 0);

    rc_write(REG_TXASK, 0x40);         // dieu che ASK 100%
    rc_write(REG_MODE, 0x3D);          // CRC khoi tao 0x6363
    rc_write(REG_RFCFG, 0x70);         // do loi anten toi da (48dB) de doc xa hon

    // 5. Bat anten (TX1, TX2: bit0, bit1 cua TxControlReg)
    tmp = rc_read(REG_TXCONTROL);
    if ((tmp & 0x03) != 0x03) rc_write(REG_TXCONTROL, (uint8_t)(tmp | 0x03));

    // 6. Kiem tra chip co tra loi khong
    ver = rc_read(REG_VERSION);
    if (ver == 0x00 || ver == 0xFF) return 0;   // khong thay chip (sai day / mat nguon)
    return 1;
}

uint8_t RC522_Request(void)
{
    uint8_t buf[2];
    uint8_t back[18];
    uint16_t bits = 0;
    uint8_t st;

    rc_write(REG_BITFRAMING, 0x07);    // gui 7 bit cuoi cua byte (lenh REQA chi 7 bit)
    buf[0] = PICC_REQIDL;
    st = rc_tocard(buf, 1, back, &bits);
    if ((st != RC522_OK) || (bits != 0x10)) {   // ATQA phai du 16 bit
        if (st == RC522_OK) st = RC522_ERR;
    }
    return st;
}

uint8_t RC522_Anticoll(uint8_t *uid)
{
    uint8_t buf[2];
    uint8_t back[18];
    uint16_t bits = 0;
    uint8_t st, i, chk;

    rc_write(REG_BITFRAMING, 0x00);
    buf[0] = PICC_ANTICOLL;
    buf[1] = 0x20;
    st = rc_tocard(buf, 2, back, &bits);

    if (st == RC522_OK) {
        if (bits != 40) {              // phai nhan du 5 byte = 40 bit
            st = RC522_ERR;
        } else {
            chk = 0;
            for (i = 0; i < 4; i++) chk ^= back[i];   // byte thu 5 la BCC = XOR 4 byte UID
            if (chk != back[4]) {
                st = RC522_ERR;
            } else {
                for (i = 0; i < 4; i++) uid[i] = back[i];
            }
        }
    }
    return st;
}

void RC522_Halt(void)
{
    uint8_t buf[4];
    uint8_t back[18];
    uint16_t bits = 0;

    buf[0] = PICC_HALT;
    buf[1] = 0x00;
    rc_calc_crc(buf, 2, &buf[2]);
    rc_tocard(buf, 4, back, &bits);    // the khong tra loi HALT nen ket qua bo qua
}
