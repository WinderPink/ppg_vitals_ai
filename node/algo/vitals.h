#ifndef VITALS_H
#define VITALS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VITALS_FS_HZ            100
#define VITALS_WINDOW_SAMPLES   800   // 8s @ 100Hz - khop voi input window cua AI model BP

#define VITALS_IBI_MIN_S        0.33f   // 180 bpm
#define VITALS_IBI_MAX_S        1.5f   // 40 bpm
#define VITALS_R_MIN            0.1f
#define VITALS_R_MAX            3.0f

/* Nguong DC toi thieu cua kenh IR de xac dinh co ngon tay dat len cam bien.
 * TODO: chinh lai theo gia tri IR= thuc te do duoc khi KHONG dat tay (xem
 * log RED=.../IR=... tho), dat cao hon muc do (vd gap 1.5-2 lan) de co bien an toan. */
#define VITALS_MIN_IR_DC        50000.0f

typedef struct {
    float hr;              // bpm
    float spo2;             // %
    bool  hr_valid;
    bool  spo2_valid;
    bool  finger_detected;  // co dat ngon tay hay khong (dua vao DC cua kenh IR)
    int   num_peaks;        // de debug/log
} vitals_result_t;

/*
 * Tinh HR + SpO2 tren 1 cua so VITALS_WINDOW_SAMPLES mau.
 *
 * red_bp/ir_bp : tin hieu SAU EMA + bandpass, CHUA qua z-score
 *                (bat buoc dung ban nay de AC con giu ty le bien do that,
 *                 z-score se pha vi ty le RED/IR can cho cong thuc SpO2)
 * red_raw/ir_raw: tin hieu tho tu cam bien (dung tinh DC + finger-detect)
 * n            : so mau trong cua so (thuong = VITALS_WINDOW_SAMPLES)
 */
void vitals_compute(const float *red_bp, const float *ir_bp,
                     const uint32_t *red_raw, const uint32_t *ir_raw,
                     int n, vitals_result_t *out);

#ifdef __cplusplus
}
#endif

#endif // VITALS_H