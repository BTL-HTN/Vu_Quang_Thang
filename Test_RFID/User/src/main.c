
#include "stm32f10x.h"
#include "MFRC522.h"
#include "BUZZER.h"
#include <stdint.h>

/* ================= RFID CONFIG ================= */

MFRC522_Config_t RFID_Config =
    {
        .SCK_Port = GPIOA,
        .SCK_Pin = GPIO_Pin_5,

        .MISO_Port = GPIOA,
        .MISO_Pin = GPIO_Pin_6,

        .MOSI_Port = GPIOA,
        .MOSI_Pin = GPIO_Pin_7,

        .CS_Port = GPIOB,
        .CS_Pin = GPIO_Pin_12,

        .RST_Port = GPIOB,
        .RST_Pin = GPIO_Pin_0,

        .Delay = 10};

/* ================= BUZZER CONFIG ================= */

Buzzer_Config_t buzzer_config =
    {
        .Port = GPIOB,
        .Pin = GPIO_Pin_8,
        .Polarity = BUZZER_ACTIVE_HIGH};

/* ================= VARIABLES ================= */

volatile uint8_t rfid_status;
volatile uint8_t rfid_version;
volatile uint8_t uid_length;
volatile uint8_t uid[MFRC522_MAX_UID_LEN];

/* ================= UART1 INIT ================= */

static void UART1_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_USART1,
                           ENABLE);

    /* PA9 - TX */
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA10 - RX */
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&uart);
    uart.USART_BaudRate = 115200;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_HardwareFlowControl =
        USART_HardwareFlowControl_None;
    uart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
}

/* ================= UART SEND ================= */

static void UART1_SendChar(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        ;

    USART_SendData(USART1, (uint16_t)c);
}

static void UART1_SendString(const char *str)
{
    while (*str)
    {
        UART1_SendChar(*str++);
    }
}

static void UART1_SendHex(uint8_t value)
{
    const char hex[] = "0123456789ABCDEF";

    UART1_SendChar(hex[(value >> 4) & 0x0F]);
    UART1_SendChar(hex[value & 0x0F]);
}

static void UART1_SendDecimal(uint32_t value)
{
    char buffer[11];
    uint8_t i = 0;

    if (value == 0)
    {
        UART1_SendChar('0');
        return;
    }

    while (value > 0 && i < sizeof(buffer))
    {
        buffer[i++] = (char)('0' + value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        UART1_SendChar(buffer[--i]);
    }
}

/* ================= MAIN ================= */

int main(void)
{
    uint8_t i;

    SystemInit();

    /* Khoi tao UART */
    UART1_Init();

    UART1_SendString("\r\n");
    UART1_SendString("================================\r\n");
    UART1_SendString(" STM32F103 RFID DEBUG SYSTEM\r\n");
    UART1_SendString("================================\r\n");

    /* Khoi tao RFID */
    UART1_SendString("[RFID] Initializing MFRC522...\r\n");
    MFRC522_Init(&RFID_Config);

    /* Khoi tao buzzer */
    UART1_SendString("[BUZZER] Initializing...\r\n");
    Buzzer_Init(&buzzer_config);

    /* Kiem tra phien ban RC522 */
    rfid_version = MFRC522_ReadVersion();

    UART1_SendString("[RFID] VersionReg = 0x");
    UART1_SendHex(rfid_version);
    UART1_SendString("\r\n");

    if (rfid_version == 0x91 || rfid_version == 0x92)
    {
        UART1_SendString("[RFID] RC522 detected OK\r\n");
    }
    else
    {
        UART1_SendString("[ERROR] RC522 version invalid!\r\n");
        UART1_SendString("[CHECK] Power, CS, SCK, MOSI, MISO, RST\r\n");
    }

    /* Bao hieu khoi dong */
    Buzzer_Beep(100);

    UART1_SendString("[SYSTEM] Ready. Please scan card.\r\n");

    while (1)
    {
        uid_length = 0;

        rfid_status = MFRC522_Check(
            (uint8_t *)uid,
            (uint8_t *)&uid_length);

        if (rfid_status == MFRC522_OK)
        {
            UART1_SendString("\r\n[RFID] Card detected!\r\n");
            UART1_SendString("[RFID] UID length: ");
            UART1_SendDecimal(uid_length);
            UART1_SendString("\r\n[RFID] UID: ");

            for (i = 0; i < uid_length; i++)
            {
                UART1_SendHex(uid[i]);

                if (i < uid_length - 1)
                    UART1_SendChar(' ');
            }

            UART1_SendString("\r\n[BUZZER] Beep!\r\n");

            /* Doc the thanh cong: keu mot tieng */
            Buzzer_Beep(100);

            UART1_SendString("[SYSTEM] Scan success\r\n");

            /* Cho mot khoang ngan truoc lan doc tiep theo */
            for (volatile uint32_t d = 0; d < 500000; d++)
                __NOP();
        }
        else if (rfid_status == MFRC522_NO_TAG)
        {
            /* Khong in log lien tuc khi khong co the */
        }
        else
        {
            UART1_SendString("[ERROR] Failed to read card, status = ");
            UART1_SendDecimal(rfid_status);
            UART1_SendString("\r\n");
        }
    }
}
