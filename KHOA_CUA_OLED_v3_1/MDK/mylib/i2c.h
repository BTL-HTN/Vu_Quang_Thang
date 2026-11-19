#include "stm32f10x.h"                  // Device header
           

#ifndef __I2C_H
#define __I2C_H

#include "stm32f10x.h"

void i2c_init(char i2c, unsigned short speed_mode);
void i2c_start(char i2c);
void i2c_add(char i2c, char address, char RW);
void i2c_data(char i2c, char data);
void i2c_stop(char i2c);
void i2c_write(char i2c, char address, char data[]);
uint8_t i2c_failed(char i2c);   // 1 = giao dich vua roi bi loi

#endif






















