#ifndef SERVO_H
#define SERVO_H

#include "stm32f10x.h"

/* Servo SG90/MG90S : chan tin hieu = PB1 (TIM3_CH4), PWM 50Hz */

void Servo_Init(void);                 // khoi tao PB1 + TIM3_CH4 (PWM 50Hz)
void Servo_Write(uint8_t angle);       // dieu khien goc tu 0 den 180 do

#endif
