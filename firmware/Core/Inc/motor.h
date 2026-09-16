/**
  ******************************************************************************
  * @file           : motor.h
  * @brief          : TB6612FNG 直流电机驱动
  *                   控制: PB1=TIM3_CH4(PWM), PC6=AIN1, PC7=AIN2, PB2=STBY
  *                   电源: 12V独立供电(VM), 3.3V逻辑(VCC)
  *                   电机: RC-370CM-21145, 12V, 14000RPM, 偏心轮1.8g
  ******************************************************************************
  */

#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 控制引脚 */
#define MOTOR_AIN1_PORT     GPIOC
#define MOTOR_AIN1_PIN      GPIO_PIN_6
#define MOTOR_AIN2_PORT     GPIOC
#define MOTOR_AIN2_PIN      GPIO_PIN_7
#define MOTOR_STBY_PORT     GPIOB
#define MOTOR_STBY_PIN      GPIO_PIN_2

/* PWM参数: TIM3, 1kHz, 0-999 */
#define MOTOR_PWM_MAX       999

/* API */

void Motor_Init(void);
void Motor_Start(void);       /* 使能 */
void Motor_Stop(void);        /* 刹车 */
void Motor_SetSpeed(uint16_t duty);  /* 0-999 */
uint16_t Motor_GetSpeed(void);       /* 当前占空比, OLED显示用 */
void Motor_Forward(uint16_t duty);
void Motor_Reverse(uint16_t duty);

#endif /* __MOTOR_H */
