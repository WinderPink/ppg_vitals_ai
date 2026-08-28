/**
 * @file    filter.h
 * @brief   Bo loc so cho PPG Filter Task: EMA, IIR Bandpass (SOS), Z-score
 * @note    Sample rate dau vao = 100Hz (da average 1 mau tu MAX30102 100Hz)
 */

#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>
#include <stdbool.h>

/* ===================== CONFIG ===================== */
#define FILTER_FS_HZ            100.0f   // tan so lay mau dau vao filter task
#define ZSCORE_WINDOW_SIZE      800     // 800 mau = 8s @ 100Hz
#define SOS_NUM_SECTIONS        2       // Butterworth bandpass bac 2 -> 2 section

/* ===================== EMA FILTER ===================== */
typedef struct {
    float   alpha;          // he so lam muot, 0 < alpha <= 1
    float   y_prev;         // gia tri output truoc do
    bool    initialized;
} ema_filter_t;

void  ema_filter_init(ema_filter_t *f, float alpha);
float ema_filter_update(ema_filter_t *f, float x);

/* ===================== IIR BANDPASS - SOS ===================== */
typedef struct {
    float b0, b1, b2;   // he so tu (numerator)
    float a1, a2;       // he so mau (denominator), a0 da chuan hoa = 1
    float z1, z2;        // bien trang thai (delay elements)
} biquad_t;

typedef struct {
    biquad_t sections[SOS_NUM_SECTIONS];
} sos_filter_t;

void  sos_filter_init(sos_filter_t *f);
float sos_filter_update(sos_filter_t *f, float x);

/* ===================== Z-SCORE NORMALIZATION ===================== */
typedef struct {
    float    buffer[ZSCORE_WINDOW_SIZE];
    uint16_t head;          // vi tri ghi tiep theo (circular)
    uint16_t count;         // so mau da co trong buffer (<= WINDOW_SIZE)
    float    sum;           // tong chay (running sum)
    float    sum_sq;        // tong binh phuong chay (running sum of squares)
} zscore_filter_t;

void  zscore_filter_init(zscore_filter_t *f);
float zscore_filter_update(zscore_filter_t *f, float x);

/* ===================== PIPELINE TONG HOP CHO FILTER TASK ===================== */
typedef struct {
    ema_filter_t     ema;
    sos_filter_t     sos;
    zscore_filter_t  zscore;
} ppg_filter_pipeline_t;

void  ppg_filter_pipeline_init(ppg_filter_pipeline_t *p, float ema_alpha);
float ppg_filter_pipeline_update(ppg_filter_pipeline_t *p, float raw_sample);
float ppg_filter_pipeline_update_ex(ppg_filter_pipeline_t *p, float raw_sample, float *bp_out);

#endif /* FILTER_H */