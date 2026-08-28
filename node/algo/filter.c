/**
 * @file    filter.c
 * @brief   Trien khai bo loc so cho PPG Filter Task (chay trong FreeRTOS task)
 *
 * Pipeline de nghi trong filter task:
 *      raw (100Hz, tu PPG task qua queue)
 *        -> EMA (khu nhieu tan so cao, muot tin hieu)
 *        -> IIR Bandpass SOS 0.5-3Hz (loai baseline wander + high-freq noise,
 *           giu lai dai tan nhip tim ~30-180 bpm)
 *        -> Z-score (chuan hoa bien do, window 800 mau ~ 8s)
 *        -> dua vao queue cho AI task
 */

#include "filter.h"
#include <math.h>
#include <string.h>

/* ===================== THONG SO THIET KE ===================== */
#define FS_HZ           100.0f
#define BPF_LOW_HZ      0.5f
#define BPF_HIGH_HZ     3.0f
/* Sinh he so bang: scipy.signal.butter(2, [0.5,3.0], btype='bandpass',
 *                                       fs=100, output='sos')
 * -> filter bandpass THUC su la bac 4 (2 section x bac 2), khong phai bac 2 */

/* ===================== EMA FILTER ===================== */

void ema_filter_init(ema_filter_t *f, float alpha)
{
    f->alpha = alpha;
    f->y_prev = 0.0f;
    f->initialized = false;
}

float ema_filter_update(ema_filter_t *f, float x)
{
    if (!f->initialized) {
        f->y_prev = x;              // khoi tao bang mau dau tien, tranh transient
        f->initialized = true;
        return x;
    }
    // y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
    float y = f->alpha * x + (1.0f - f->alpha) * f->y_prev;
    f->y_prev = y;
    return y;
}

/* ===================== IIR BANDPASS - SOS =====================
 * Butterworth bandpass, dai 0.5 - 3.0 Hz, fs = 100 Hz, tong bac = 4
 * (scipy.signal.butter(2, [0.5,3.0], btype='bandpass', fs=100, output='sos'))
 * Dang Direct Form II Transposed cho tung bien the (on dinh so hoc tot hon Direct Form I)
 * Chay causal, tung mau mot -> phu hop real-time, khac voi filtfilt (offline, non-causal)
 */

void sos_filter_init(sos_filter_t *f)
{
    // Section 0
    f->sections[0].b0 = 0.0055427172f;
    f->sections[0].b1 = 0.0110854344f;
    f->sections[0].b2 = 0.0055427172f;
    f->sections[0].a1 = -1.7785987160f;
    f->sections[0].a2 =  0.8007703859f;
    f->sections[0].z1 = 0.0f;
    f->sections[0].z2 = 0.0f;

    // Section 1
    f->sections[1].b0 =  1.0000000000f;
    f->sections[1].b1 = -2.0000000000f;
    f->sections[1].b2 =  1.0000000000f;
    f->sections[1].a1 = -1.9329241926f;
    f->sections[1].a2 =  0.9355294025f;
    f->sections[1].z1 = 0.0f;
    f->sections[1].z2 = 0.0f;
}

// Xu ly 1 mau qua 1 bien the (Direct Form II Transposed)
static inline float biquad_process(biquad_t *bq, float x)
{
    float y = bq->b0 * x + bq->z1;
    bq->z1  = bq->b1 * x - bq->a1 * y + bq->z2;
    bq->z2  = bq->b2 * x - bq->a2 * y;
    return y;
}

float sos_filter_update(sos_filter_t *f, float x)
{
    float y = x;
    for (int i = 0; i < SOS_NUM_SECTIONS; i++) {
        y = biquad_process(&f->sections[i], y);
    }
    return y;
}

/* ===================== Z-SCORE NORMALIZATION (window 800 mau) =====================
 * z[n] = (x[n] - mean_window) / std_window
 * Dung running sum & running sum-of-squares de tinh O(1) moi mau.
 *
 * LUU Y NUMERICAL STABILITY:
 * Cong don sum/sum_sq bang float32 lien tuc (thiet bi chay hang gio) se bi
 * troi (drift) do sai so lam tron tich luy. Vi buffer 800 mau da luu san
 * gia tri goc, ta dinh ky (moi khi buffer quay het 1 vong) tinh lai
 * sum/sum_sq CHINH XAC tu buffer thay vi tin tuong hoan toan phep cong don
 * -> khong ton them RAM, chi ton O(window) moi ZSCORE_WINDOW_SIZE mau.
 */

#define ZSCORE_MIN_SAMPLES  (int)(1.0f * FS_HZ)  // ~1s dau: chua du tin cay de chuan hoa

void zscore_filter_init(zscore_filter_t *f)
{
    memset(f->buffer, 0, sizeof(f->buffer));
    f->head = 0;
    f->count = 0;
    f->sum = 0.0f;
    f->sum_sq = 0.0f;
}

static void zscore_resync(zscore_filter_t *f)
{
    float sum = 0.0f, sum_sq = 0.0f;
    for (int i = 0; i < f->count; i++) {
        float v = f->buffer[i];
        sum    += v;
        sum_sq += v * v;
    }
    f->sum    = sum;
    f->sum_sq = sum_sq;
}

float zscore_filter_update(zscore_filter_t *f, float x)
{
    if (f->count < ZSCORE_WINDOW_SIZE) {
        // buffer chua day: chi them mau moi
        f->buffer[f->head] = x;
        f->sum    += x;
        f->sum_sq += x * x;
        f->count++;
    } else {
        // buffer day: loai mau cu nhat, them mau moi (circular)
        float old = f->buffer[f->head];
        f->sum    += x - old;
        f->sum_sq += x * x - old * old;
        f->buffer[f->head] = x;
    }
    f->head = (f->head + 1) % ZSCORE_WINDOW_SIZE;

    // Dinh ky resync tu buffer de chong troi sai so tich luy (moi khi quay het 1 vong)
    if (f->head == 0) {
        zscore_resync(f);
    }

    float n = (float)f->count;
    float mean = f->sum / n;
    float var  = (f->sum_sq / n) - (mean * mean);
    if (var < 1e-6f) var = 1e-6f;     // tranh chia 0 khi tin hieu phang (VD luc khoi dong)
    float std = sqrtf(var);

    // Giai doan warm-up (~1s dau): thong ke chua du tin cay, tra ve 0 thay vi gia tri giat cuc
    if (f->count < ZSCORE_MIN_SAMPLES) {
        return 0.0f;
    }

    return (x - mean) / std;
}

/* ===================== PIPELINE TONG HOP ===================== */

void ppg_filter_pipeline_init(ppg_filter_pipeline_t *p, float ema_alpha)
{
    ema_filter_init(&p->ema, ema_alpha);
    sos_filter_init(&p->sos);
    zscore_filter_init(&p->zscore);
}

float ppg_filter_pipeline_update_ex(ppg_filter_pipeline_t *p, float raw_sample, float *bp_out)
{
    float y1 = ema_filter_update(&p->ema, raw_sample);
    float y2 = sos_filter_update(&p->sos, y1);   // bandpass-only, chua z-score
    float y3 = zscore_filter_update(&p->zscore, y2);

    if (bp_out != NULL) {
        *bp_out = y2;
    }
    return y3;
}

// Giu nguyen ham cu de khong pha vo code cho khac dang dung
float ppg_filter_pipeline_update(ppg_filter_pipeline_t *p, float raw_sample)
{
    return ppg_filter_pipeline_update_ex(p, raw_sample, NULL);
}