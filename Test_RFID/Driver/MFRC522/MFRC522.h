
#ifndef __MFRC522_H
#define __MFRC522_H

#include "stm32f10x.h"
#include <stdint.h>

#define MFRC522_OK 0
#define MFRC522_ERROR 1
#define MFRC522_NO_TAG 2
#define MFRC522_COLLISION 3
#define MFRC522_TIMEOUT 4

#define MFRC522_MAX_UID_LEN 10

typedef struct
{
    GPIO_TypeDef *SCK_Port;
    uint16_t SCK_Pin;

    GPIO_TypeDef *MISO_Port;
    uint16_t MISO_Pin;

    GPIO_TypeDef *MOSI_Port;
    uint16_t MOSI_Pin;

    GPIO_TypeDef *CS_Port;
    uint16_t CS_Pin;

    GPIO_TypeDef *RST_Port;
    uint16_t RST_Pin;

    uint16_t Delay;
} MFRC522_Config_t;

void MFRC522_Init(MFRC522_Config_t *config);
void MFRC522_Reset(void);

uint8_t MFRC522_ReadVersion(void);
uint8_t MFRC522_ReadRegister(uint8_t reg);
void MFRC522_WriteRegister(uint8_t reg, uint8_t value);

uint8_t MFRC522_Request(void);
uint8_t MFRC522_IsNewCardPresent(void);

uint8_t MFRC522_ReadCardSerial(uint8_t *uid,
                               uint8_t *uidLength);

uint8_t MFRC522_Check(uint8_t *uid,
                      uint8_t *uidLength);

#endif
