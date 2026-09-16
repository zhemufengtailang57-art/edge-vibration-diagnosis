/**
  ******************************************************************************
  * @file           : fft_analyzer.c
  * @brief          : FFT频谱分析实现
  *                   管线: int16 raw → float → DC remove → Hanning → RFFT → mag
  *                   F407 FPU硬件加速，128点FFT约需0.5ms (非ISR上下文)
  ******************************************************************************
  */

#include "fft_analyzer.h"
#include "log.h"
#include <math.h>

/* ==================== CMSIS-DSP RFFT实例 ==================== */
static arm_rfft_fast_instance_f32 rfft_inst;

/* 窗函数: Hanning (汉宁窗) — 降低频谱泄漏 */
static float hanning_window[FFT_SIZE];

/* 临时缓冲区 */
static float   fft_input[FFT_SIZE * 2];   /* RFFT输出需要2*N空间 */
static float   fft_mag[FFT_SIZE / 2 + 1]; /* 幅值谱 (含Nyquist) */

/* ==================== FFT管线 ==================== */

void FFT_Analyzer_Init(void)
{
    /* 初始化CMSIS-DSP RFFT实例 */
    arm_rfft_fast_init_f32(&rfft_inst, FFT_SIZE);

    /* 预计算Hanning窗 */
    for (uint16_t i = 0; i < FFT_SIZE; i++)
    {
        hanning_window[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (FFT_SIZE - 1)));
    }

    LOG_INFO("FFT", "Init OK, N=%d, Fs=%.0fHz, res=%.1fHz/bin",
             FFT_SIZE, FFT_FS, FFT_RESOLUTION);
}

/**
  * @brief  管线: raw → float → DC → Hanning → RFFT → magnitude → peak
  *         耗时: ~650µs @168MHz (FPU加速)
  */
void FFT_Analyze(const int16_t *data, uint32_t count, FFT_Result *result)
{
    float   mean = 0.0f;
    float   max_mag = 0.0f;
    uint16_t peak_bin = 0;
    uint16_t n = (count < FFT_SIZE) ? count : FFT_SIZE;

    /* 清零 */
    memset(fft_input,  0, sizeof(fft_input));
    memset(fft_mag,    0, sizeof(fft_mag));
    result->valid = 0;

    if (n < 16) return;  /* 至少16个采样点 */

    /* === 1. 计算DC均值 === */
    for (uint16_t i = 0; i < n; i++)
        mean += (float)data[i];
    mean /= (float)n;

    /* === 2. 去DC + 加Hanning窗 + float转换 === */
    for (uint16_t i = 0; i < n; i++)
    {
        fft_input[i] = ((float)data[i] - mean) * hanning_window[i];
    }

    /* === 3. 实数FFT (CMSIS-DSP, FPU硬件加速) === */
    /*     arm_rfft_fast_f32:
     *       input:  fft_input (实部)
     *       output: fft_input (复用, 格式: {real[0], real[1], ..., imag[1], imag[2]...}
     *       pos=0: DC, pos=FFT_SIZE/2: Nyquist
     */
    arm_rfft_fast_f32(&rfft_inst, fft_input, fft_input, 0);

    /* === 4. 计算幅值谱 (取模) === */
    /*     RFFT输出在 fft_input 中的布局:
     *       [0]     = DC real值
     *       [FFT_SIZE] = Nyquist bin的imag (因为是实数FFT, Nyquist的imag存在这里)
     *       [1][2]  = bin1 (real, imag)
     *       [3][4]  = bin2 (real, imag)
     *        ...
     *       [FFT_SIZE-2][FFT_SIZE-1] = bin(N/2-1) (real, imag)
     */

    /* DC分量 (bin 0) */
    fft_mag[0] = fabsf(fft_input[0]) / (float)FFT_SIZE;

    /* 中间bins (1 到 N/2-1) */
    for (uint16_t i = 1; i < FFT_SIZE / 2; i++)
    {
        float real = fft_input[2 * i];
        float imag = fft_input[2 * i + 1];
        fft_mag[i] = sqrtf(real * real + imag * imag) / (float)FFT_SIZE;
    }

    /* Nyquist bin */
    fft_mag[FFT_SIZE / 2] = fabsf(fft_input[1]) / (float)FFT_SIZE;

    /* === 5. 找峰值 (跳过DC bin0, 跳过Nyquist bin) === */
    for (uint16_t i = 1; i < FFT_SIZE / 2; i++)
    {
        if (fft_mag[i] > max_mag)
        {
            max_mag  = fft_mag[i];
            peak_bin = i;
        }
    }

    /* === 6. 计算RMS === */
    float sum_sq = 0.0f;
    for (uint16_t i = 0; i < n; i++)
    {
        float v = (float)data[i] - mean;
        sum_sq += v * v;
    }
    float rms = sqrtf(sum_sq / (float)n);

    /* ADXL345 ±16g全分辨率: 3.9mg/LSB → 1g = 256 LSB */
    /* 幅值单位换算: LSB → g */
    float scale = 1.0f / 256.0f;

    /* === 7. 输出结果 === */
    result->rms        = rms * scale;

    /* 阈值判断: RMS < 10mg 视为静止, 不报虚假峰值 */
    if (result->rms < 0.01f)
    {
        result->freq      = 0.0f;
        result->amplitude = 0.0f;
        result->peak_bin  = 0;
        result->valid     = 0;
    }
    else
    {
        result->freq      = peak_bin * FFT_RESOLUTION;
        result->amplitude = max_mag * scale;
        result->peak_bin  = peak_bin;
        result->valid     = 1;
    }

    memcpy(result->spectrum, fft_mag, sizeof(fft_mag));
}

float FFT_GetAmplitudeAt(const FFT_Result *result, float freq)
{
    uint16_t bin = (uint16_t)(freq / FFT_RESOLUTION + 0.5f);
    if (bin >= FFT_SIZE / 2) return 0.0f;
    return result->spectrum[bin];
}
