/**
  ******************************************************************************
  * @file           : ai_classifier.c
  * @brief          : 振动故障分类MLP推理 (int8量化权重)
  *                   结构: 5输入 -> 16隐藏(ReLU) -> 8隐藏(ReLU) -> 3输出
  *                   推理时间: ~20us @168MHz
  ******************************************************************************
  */

#include "ai_classifier.h"
#include "ai_model.h"
#include <math.h>
#include <stddef.h>

/* ==================== 初始化 ==================== */

void AI_Classifier_Init(void)
{
    /* 无状态 */
}

/* ==================== 推理 (int8权重 + float累加) ==================== */

uint8_t AI_Classify(const float *input, float *confidence)
{
    float h1[AI_HIDDEN1_SIZE];
    float h2[AI_HIDDEN2_SIZE];
    float out[AI_OUTPUT_SIZE];
    uint8_t i, j;

    /* ---- 输入归一化 ---- */
    float x[AI_INPUT_SIZE];
    for (i = 0; i < AI_INPUT_SIZE; i++)
    {
        x[i] = (input[i] - AI_MEAN[i]) / AI_STD[i];
    }

    /* ---- 第1层: 5 -> 16 (int8权重, ReLU) ---- */
    for (i = 0; i < AI_HIDDEN1_SIZE; i++)
    {
        float sum = 0.0f;
        for (j = 0; j < AI_INPUT_SIZE; j++)
            sum += (float)AI_W1[i][j] * x[j];
        sum = sum * AI_W1_SCALE + (float)AI_B1[i] * AI_B1_SCALE;
        h1[i] = (sum > 0.0f) ? sum : 0.0f;
    }

    /* ---- 第2层: 16 -> 8 (int8权重, ReLU) ---- */
    for (i = 0; i < AI_HIDDEN2_SIZE; i++)
    {
        float sum = 0.0f;
        for (j = 0; j < AI_HIDDEN1_SIZE; j++)
            sum += (float)AI_W2[i][j] * h1[j];
        sum = sum * AI_W2_SCALE + (float)AI_B2[i] * AI_B2_SCALE;
        h2[i] = (sum > 0.0f) ? sum : 0.0f;
    }

    /* ---- 输出层: 8 -> 3 ---- */
    uint8_t best_class = 0;
    float best_val = -1e30f;
    for (i = 0; i < AI_OUTPUT_SIZE; i++)
    {
        float sum = 0.0f;
        for (j = 0; j < AI_HIDDEN2_SIZE; j++)
            sum += (float)AI_W3[i][j] * h2[j];
        sum = sum * AI_W3_SCALE + (float)AI_B3[i] * AI_B3_SCALE;
        out[i] = sum;
        if (sum > best_val)
        {
            best_val = sum;
            best_class = i;
        }
    }

    /* ---- softmax置信度 ---- */
    if (confidence != NULL)
    {
        float max_val = out[0];
        for (i = 1; i < AI_OUTPUT_SIZE; i++)
            if (out[i] > max_val) max_val = out[i];

        float exp_sum = 0.0f;
        float exp_val[AI_OUTPUT_SIZE];
        for (i = 0; i < AI_OUTPUT_SIZE; i++)
        {
            exp_val[i] = expf(out[i] - max_val);
            exp_sum += exp_val[i];
        }
        *confidence = exp_val[best_class] / exp_sum;
    }

    return best_class;
}
