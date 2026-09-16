/**
  ******************************************************************************
  * @file           : motor.c
  * @brief          : TB6612FNG 电机驱动实现
  *                   IN1/IN2 → 方向, PWM → 速度, STBY → 使能
  ******************************************************************************
  */

#include "motor.h"

extern TIM_HandleTypeDef htim3;

static uint16_t g_motor_duty = 0;   /* 当前PWM占空比(0-999), 供OLED显示 */

void Motor_Init(void)
{
    /* 初始状态: 刹车+待机 */
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);

    /* 启动PWM: TIM3_CH4, 占空比0 */
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
}

void Motor_Start(void)
{
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);
}

void Motor_Stop(void)
{
    /* 刹车: IN1=LOW, IN2=LOW, PWM=0 */
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, 0);
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);
    g_motor_duty = 0;
}

void Motor_SetSpeed(uint16_t duty)
{
    if (duty > MOTOR_PWM_MAX) duty = MOTOR_PWM_MAX;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, duty);
    g_motor_duty = duty;
}

uint16_t Motor_GetSpeed(void)
{
    return g_motor_duty;
}

void Motor_Forward(uint16_t duty)
{
    Motor_Start();
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_SET);
    Motor_SetSpeed(duty);
}

void Motor_Reverse(uint16_t duty)
{
    Motor_Start();
    HAL_GPIO_WritePin(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, GPIO_PIN_SET);
    Motor_SetSpeed(duty);
}
