/**
  ******************************************************************************
  * @file           : ai_classifier.h
  * @brief          : 振动故障分类MLP推理
  *                   结构: 17输入 -> 32隐藏(ReLU) -> 16隐藏(ReLU) -> 3输出
  *                   输入: RMS(mG) + 16个FFT频点能量
  *                   输出: 0=正常, 1=螺丝松动, 2=不平衡
  *                   推理时间: ~50us @168MHz (纯浮点, F407有FPU)
  ******************************************************************************
  */

#ifndef __AI_CLASSIFIER_H
#define __AI_CLASSIFIER_H

#include <stdint.h>

/* 分类结果标签 */
#define AI_CLASS_NORMAL     0  /* 正常 */
#define AI_CLASS_LOOSE      1  /* 螺丝松动 */
#define AI_CLASS_UNBALANCE  2  /* 不平衡 */

/**
  * @brief  初始化分类器 (无状态, 占位)
  */
void AI_Classifier_Init(void);

/**
  * @brief  运行推理
  * @param  input: 17维特征数组
  *         [0] = RMS (mG)
  *         [1..16] = FFT频点能量 (幅值*500)
  * @param  confidence: 输出置信度(可选, NULL忽略)
  * @retval 分类结果: 0=正常, 1=松动, 2=不平衡
  */
uint8_t AI_Classify(const float *input, float *confidence);

#endif /* __AI_CLASSIFIER_H */
