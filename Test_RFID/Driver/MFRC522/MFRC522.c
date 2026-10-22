
#include "MFRC522.h"

/* MFRC522 registers */
#define CommandReg 0x01
#define ComIEnReg 0x02
#define DivIEnReg 0x03
#define ComIrqReg 0x04
#define DivIrqReg 0x05
#define ErrorReg 0x06
#define Status2Reg 0x08
#define FIFODataReg 0x09
#define FIFOLevelReg 0x0A
#define ControlReg 0x0C
#define BitFramingReg 0x0D
#define CollReg 0x0E
#define ModeReg 0x11
#define TxControlReg 0x14
#define TxASKReg 0x15
#define CRCResultRegH 0x21
#define CRCResultRegL 0x22
#define TModeReg 0x2A
#define TPrescalerReg 0x2B
#define TReloadRegH 0x2C
#define TReloadRegL 0x2D
#define VersionReg 0x37

/* PCD commands */
#define PCD_IDLE 0x00
#define PCD_CALCCRC 0x03
#define PCD_TRANSCEIVE 0x0C
#define PCD_SOFTRESET 0x0F

/* PICC commands */
#define PICC_REQIDL 0x26
#define PICC_ANTICOLL 0x20
#define PICC_SELECT 0x70
#define PICC_CT 0x88

#define PICC_SEL_CL1 0x93
#define PICC_SEL_CL2 0x95
#define PICC_SEL_CL3 0x97

#define MI_OK 0
#define MI_NOTAGERR 1
#define MI_ERR 2

#define FIFO_SIZE 64

static MFRC522_Config_t *rfid;

static void DelayCycles(uint32_t count)
{
    volatile uint32_t i;

    for (i = 0; i < count; i++)
        __NOP();
}

static void SPI_Delay(void)
{
    DelayCycles(rfid->Delay);
}

static uint8_t PinRead(GPIO_TypeDef *port, uint16_t pin)
{
    return GPIO_ReadInputDataBit(port, pin) == Bit_SET;
}

static void CS_Low(void)
{
    GPIO_ResetBits(rfid->CS_Port, rfid->CS_Pin);
}

static void CS_High(void)
{
    GPIO_SetBits(rfid->CS_Port, rfid->CS_Pin);
}

static void RST_Low(void)
{
    GPIO_ResetBits(rfid->RST_Port, rfid->RST_Pin);
}

static void RST_High(void)
{
    GPIO_SetBits(rfid->RST_Port, rfid->RST_Pin);
}

static void SCK_Low(void)
{
    GPIO_ResetBits(rfid->SCK_Port, rfid->SCK_Pin);
}

static void SCK_High(void)
{
    GPIO_SetBits(rfid->SCK_Port, rfid->SCK_Pin);
}

static void MOSI_Low(void)
{
    GPIO_ResetBits(rfid->MOSI_Port, rfid->MOSI_Pin);
}

static void MOSI_High(void)
{
    GPIO_SetBits(rfid->MOSI_Port, rfid->MOSI_Pin);
}

static uint8_t SPI_Transfer(uint8_t data)
{
    uint8_t received = 0;
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
            MOSI_High();
        else
            MOSI_Low();

        SPI_Delay();
        SCK_High();
        SPI_Delay();

        received <<= 1;

        if (PinRead(rfid->MISO_Port, rfid->MISO_Pin))
            received |= 1;

        SCK_Low();
        data <<= 1;
    }

    return received;
}

static void RCC_EnableGPIO(GPIO_TypeDef *port)
{
    if (port == GPIOA)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    else if (port == GPIOB)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    else if (port == GPIOC)
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
}

static void GPIO_OutputInit(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = pin;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(port, &gpio);
}

static void GPIO_InputInit(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = pin;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;

    GPIO_Init(port, &gpio);
}

void MFRC522_WriteRegister(uint8_t reg, uint8_t value)
{
    CS_Low();

    SPI_Transfer((uint8_t)((reg << 1) & 0x7E));
    SPI_Transfer(value);

    CS_High();
}

uint8_t MFRC522_ReadRegister(uint8_t reg)
{
    uint8_t value;

    CS_Low();

    SPI_Transfer((uint8_t)(((reg << 1) & 0x7E) | 0x80));
    value = SPI_Transfer(0x00);

    CS_High();

    return value;
}

static void SetBitMask(uint8_t reg, uint8_t mask)
{
    MFRC522_WriteRegister(
        reg,
        MFRC522_ReadRegister(reg) | mask);
}

static void ClearBitMask(uint8_t reg, uint8_t mask)
{
    MFRC522_WriteRegister(
        reg,
        MFRC522_ReadRegister(reg) & (uint8_t)(~mask));
}

void MFRC522_Reset(void)
{
    MFRC522_WriteRegister(CommandReg, PCD_SOFTRESET);

    for (volatile uint32_t i = 0; i < 100000; i++)
    {
        if (!(MFRC522_ReadRegister(CommandReg) &
              (1U << 4)))
            break;
    }
}

static void AntennaOn(void)
{
    if ((MFRC522_ReadRegister(TxControlReg) & 0x03) != 0x03)
        SetBitMask(TxControlReg, 0x03);
}

void MFRC522_Init(MFRC522_Config_t *config)
{
    if (config == 0)
        return;

    rfid = config;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    RCC_EnableGPIO(rfid->SCK_Port);
    RCC_EnableGPIO(rfid->MISO_Port);
    RCC_EnableGPIO(rfid->MOSI_Port);
    RCC_EnableGPIO(rfid->CS_Port);
    RCC_EnableGPIO(rfid->RST_Port);

    GPIO_OutputInit(rfid->SCK_Port, rfid->SCK_Pin);
    GPIO_InputInit(rfid->MISO_Port, rfid->MISO_Pin);
    GPIO_OutputInit(rfid->MOSI_Port, rfid->MOSI_Pin);
    GPIO_OutputInit(rfid->CS_Port, rfid->CS_Pin);
    GPIO_OutputInit(rfid->RST_Port, rfid->RST_Pin);

    SCK_Low();
    MOSI_Low();
    CS_High();
    RST_High();

    DelayCycles(10000);

    RST_Low();
    DelayCycles(10000);
    RST_High();
    DelayCycles(10000);

    MFRC522_Reset();

    MFRC522_WriteRegister(TModeReg, 0x8D);
    MFRC522_WriteRegister(TPrescalerReg, 0x3E);
    MFRC522_WriteRegister(TReloadRegL, 30);
    MFRC522_WriteRegister(TReloadRegH, 0);
    MFRC522_WriteRegister(TxASKReg, 0x40);
    MFRC522_WriteRegister(ModeReg, 0x3D);

    AntennaOn();
}

uint8_t MFRC522_ReadVersion(void)
{
    return MFRC522_ReadRegister(VersionReg);
}

/* Hardware CRC_A calculation inside MFRC522 */
static uint8_t CalculateCRC(uint8_t *data,
                            uint8_t length,
                            uint8_t *result)
{
    uint8_t i;

    MFRC522_WriteRegister(CommandReg, PCD_IDLE);
    MFRC522_WriteRegister(DivIrqReg, 0x04);
    SetBitMask(FIFOLevelReg, 0x80);

    for (i = 0; i < length; i++)
        MFRC522_WriteRegister(FIFODataReg, data[i]);

    MFRC522_WriteRegister(CommandReg, PCD_CALCCRC);

    for (i = 0; i < 255; i++)
    {
        if (MFRC522_ReadRegister(DivIrqReg) & 0x04)
        {
            result[0] = MFRC522_ReadRegister(CRCResultRegL);
            result[1] = MFRC522_ReadRegister(CRCResultRegH);

            MFRC522_WriteRegister(CommandReg, PCD_IDLE);
            return MI_OK;
        }
    }

    MFRC522_WriteRegister(CommandReg, PCD_IDLE);
    return MI_ERR;
}

static uint8_t ToCard(uint8_t command,
                      uint8_t *sendData,
                      uint8_t sendLen,
                      uint8_t *backData,
                      uint16_t *backBits)
{
    uint8_t irqEn = 0;
    uint8_t waitIRq = 0;
    uint8_t irq;
    uint8_t error;
    uint8_t count;
    uint8_t lastBits;
    uint16_t i;

    if (command == PCD_TRANSCEIVE)
    {
        irqEn = 0x77;
        waitIRq = 0x30;
    }

    MFRC522_WriteRegister(ComIEnReg, irqEn | 0x80);
    ClearBitMask(ComIrqReg, 0x80);
    SetBitMask(FIFOLevelReg, 0x80);
    MFRC522_WriteRegister(CommandReg, PCD_IDLE);

    for (i = 0; i < sendLen; i++)
        MFRC522_WriteRegister(FIFODataReg, sendData[i]);

    MFRC522_WriteRegister(CommandReg, command);

    if (command == PCD_TRANSCEIVE)
        SetBitMask(BitFramingReg, 0x80);

    i = 2000;

    do
    {
        irq = MFRC522_ReadRegister(ComIrqReg);
        i--;
    } while (i && !(irq & 0x01) && !(irq & waitIRq));

    ClearBitMask(BitFramingReg, 0x80);

    if (i == 0)
        return MI_ERR;

    error = MFRC522_ReadRegister(ErrorReg);

    if (error & 0x08)
        return MI_ERR;

    if (error & 0x04)
        return MI_ERR;

    if (error & 0x10)
        return MI_ERR;

    if (error & 0x01)
        return MI_ERR;

    count = MFRC522_ReadRegister(FIFOLevelReg);

    if (count > FIFO_SIZE)
        return MI_ERR;

    lastBits = MFRC522_ReadRegister(ControlReg) & 0x07;

    if (lastBits)
        *backBits = (uint16_t)((count - 1) * 8 + lastBits);
    else
        *backBits = (uint16_t)(count * 8);

    for (i = 0; i < count; i++)
        backData[i] = MFRC522_ReadRegister(FIFODataReg);

    return MI_OK;
}

uint8_t MFRC522_Request(void)
{
    uint8_t command = PICC_REQIDL;
    uint8_t response[2];
    uint16_t bits = 0;

    MFRC522_WriteRegister(BitFramingReg, 0x07);

    if (ToCard(PCD_TRANSCEIVE, &command, 1,
               response, &bits) != MI_OK)
        return MFRC522_NO_TAG;

    if (bits != 16)
        return MFRC522_ERROR;

    return MFRC522_OK;
}

uint8_t MFRC522_IsNewCardPresent(void)
{
    return MFRC522_Request() == MFRC522_OK;
}

/* Select one UID cascade level */
static uint8_t SelectLevel(uint8_t selectCode,
                           uint8_t *uidPart,
                           uint8_t *sak)
{
    uint8_t buffer[9];
    uint8_t response[3];
    uint8_t crc[2];
    uint16_t bits = 0;
    uint8_t i;

    buffer[0] = selectCode;
    buffer[1] = PICC_SELECT;

    for (i = 0; i < 5; i++)
        buffer[2 + i] = uidPart[i];

    if (CalculateCRC(buffer, 7, crc) != MI_OK)
        return MFRC522_ERROR;

    buffer[7] = crc[0];
    buffer[8] = crc[1];

    if (ToCard(PCD_TRANSCEIVE, buffer, 9,
               response, &bits) != MI_OK)
        return MFRC522_ERROR;

    if (bits != 24)
        return MFRC522_ERROR;

    if (CalculateCRC(response, 1, crc) != MI_OK)
        return MFRC522_ERROR;

    if (response[1] != crc[0] || response[2] != crc[1])
        return MFRC522_ERROR;

    *sak = response[0];

    return MFRC522_OK;
}

static uint8_t ReadCascadeLevel(uint8_t selectCode,
                                uint8_t *uid,
                                uint8_t *uidLength,
                                uint8_t *sak)
{
    uint8_t command[2];
    uint8_t response[5];
    uint8_t uidPart[5];
    uint8_t crc[2];
    uint16_t bits = 0;
    uint8_t i;
    uint8_t result;

    command[0] = selectCode;
    command[1] = 0x20;

    MFRC522_WriteRegister(BitFramingReg, 0x00);

    result = ToCard(PCD_TRANSCEIVE, command, 2,
                    response, &bits);

    if (result != MI_OK || bits != 40)
        return MFRC522_ERROR;

    for (i = 0; i < 4; i++)
        uidPart[i] = response[i];

    uidPart[4] = response[4];

    if ((uint8_t)(uidPart[0] ^ uidPart[1] ^
                  uidPart[2] ^ uidPart[3]) != uidPart[4])
        return MFRC522_ERROR;

    if (SelectLevel(selectCode, uidPart, sak) != MFRC522_OK)
        return MFRC522_ERROR;

    if (uidPart[0] == PICC_CT)
    {
        for (i = 1; i < 4; i++)
        {
            if (*uidLength >= MFRC522_MAX_UID_LEN)
                return MFRC522_ERROR;

            uid[(*uidLength)++] = uidPart[i];
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            if (*uidLength >= MFRC522_MAX_UID_LEN)
                return MFRC522_ERROR;

            uid[(*uidLength)++] = uidPart[i];
        }
    }

    (void)crc;
    return MFRC522_OK;
}

uint8_t MFRC522_ReadCardSerial(uint8_t *uid,
                               uint8_t *uidLength)
{
    uint8_t sak = 0;
    uint8_t result;

    if (uid == 0 || uidLength == 0)
        return MFRC522_ERROR;

    *uidLength = 0;

    if (MFRC522_Request() != MFRC522_OK)
        return MFRC522_NO_TAG;

    result = ReadCascadeLevel(PICC_SEL_CL1, uid,
                              uidLength, &sak);

    if (result != MFRC522_OK)
        return result;

    if (sak & 0x04)
    {
        result = ReadCascadeLevel(PICC_SEL_CL2, uid,
                                  uidLength, &sak);

        if (result != MFRC522_OK)
            return result;
    }

    if (sak & 0x04)
    {
        result = ReadCascadeLevel(PICC_SEL_CL3, uid,
                                  uidLength, &sak);

        if (result != MFRC522_OK)
            return result;
    }

    if (sak & 0x04)
        return MFRC522_ERROR;

    return MFRC522_OK;
}

uint8_t MFRC522_Check(uint8_t *uid,
                      uint8_t *uidLength)
{
    return MFRC522_ReadCardSerial(uid, uidLength);
}
