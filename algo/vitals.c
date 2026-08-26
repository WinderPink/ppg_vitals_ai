#include "vitals.h"
#include <math.h>
#include <stdlib.h>

#define VITALS_MAX_PEAKS   64   // du cho HR toi 180bpm trong cua so 8s

/* So sanh dung cho qsort khi tinh median */
static int cmp_float(const void *a, const void *b)
{
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static float median_of(float *arr, int n)
{
    qsort(arr, n, sizeof(float), cmp_float);
    if (n % 2 == 1) {
        return arr[n / 2];
    }
    return 0.5f * (arr[n / 2 - 1] + arr[n / 2]);
}

/* Tim peak tren kenh IR_bp bang nguong thich nghi cuc bo (local adaptive threshold) */
static int find_ir_peaks(const float *ir_bp, int n, int *peak_idx)
{
    float peak_val = ir_bp[0], mean_v = 0.0f;
    for (int i = 0; i < n; i++) {
        mean_v += ir_bp[i];
        if (ir_bp[i] > peak_val) peak_val = ir_bp[i];
    }
    mean_v /= (float)n;
    float threshold = mean_v + 0.3f * (peak_val - mean_v);

    int min_dist  = (int)(0.4f * (float)VITALS_FS_HZ);   // toi thieu 0.4s giua 2 dinh
    int last_peak = -min_dist;
    int num_peaks = 0;

    for (int i = 1; i < n - 1 && num_peaks < VITALS_MAX_PEAKS; i++) {
        if (ir_bp[i] >  threshold      &&
            ir_bp[i] >  ir_bp[i - 1]   &&
            ir_bp[i] >= ir_bp[i + 1]   &&
            (i - last_peak) >= min_dist)
        {
            peak_idx[num_peaks++] = i;
            last_peak = i;
        }
    }
    return num_peaks;
}

static bool calc_hr(const int *peak_idx, int num_peaks, float *hr_out)
{
    if (num_peaks < 2) return false;

    float ibi_sum = 0.0f;
    int   ibi_count = 0;

    for (int i = 1; i < num_peaks; i++) {
        float ibi_s = (float)(peak_idx[i] - peak_idx[i - 1]) / (float)VITALS_FS_HZ;
        if (ibi_s >= VITALS_IBI_MIN_S && ibi_s <= VITALS_IBI_MAX_S) {
            ibi_sum += ibi_s;
            ibi_count++;
        }
    }
    if (ibi_count == 0) return false;

    *hr_out = 60.0f / (ibi_sum / (float)ibi_count);
    return true;
}

static bool calc_spo2(const float *red_bp, const float *ir_bp,
                       const uint32_t *red_raw, const uint32_t *ir_raw,
                       const int *peak_idx, int num_peaks, float *spo2_out)
{
    if (num_peaks < 2) return false;

    float r_values[VITALS_MAX_PEAKS];
    int   r_count = 0;

    for (int i = 0; i < num_peaks - 1; i++) {
        int i_start = peak_idx[i];
        int i_end   = peak_idx[i + 1];

        float red_max = red_bp[i_start], red_min = red_bp[i_start];
        float ir_max  = ir_bp[i_start],  ir_min  = ir_bp[i_start];
        float dc_red_sum = 0.0f, dc_ir_sum = 0.0f;

        for (int k = i_start; k <= i_end; k++) {
            if (red_bp[k] > red_max) red_max = red_bp[k];
            if (red_bp[k] < red_min) red_min = red_bp[k];
            if (ir_bp[k]  > ir_max)  ir_max  = ir_bp[k];
            if (ir_bp[k]  < ir_min)  ir_min  = ir_bp[k];
            dc_red_sum += (float)red_raw[k];
            dc_ir_sum  += (float)ir_raw[k];
        }

        int   count  = i_end - i_start + 1;
        float ac_red = red_max - red_min;
        float ac_ir  = ir_max - ir_min;
        float dc_red = dc_red_sum / (float)count;
        float dc_ir  = dc_ir_sum  / (float)count;

        if (ac_ir > 0.0f && dc_red > 0.0f && dc_ir > 0.0f) {
            float r = (ac_red / dc_red) / (ac_ir / dc_ir);
            if (r >= VITALS_R_MIN && r <= VITALS_R_MAX && r_count < VITALS_MAX_PEAKS) {
                r_values[r_count++] = r;
            }
        }
    }

    if (r_count == 0) return false;

    float r_med = median_of(r_values, r_count);   // median ben vung hon mean truoc outlier
    float spo2 = 110.0f - 25.0f * r_med;
    if (spo2 < 0.0f)   spo2 = 0.0f;
    if (spo2 > 100.0f) spo2 = 100.0f;

    *spo2_out = spo2;
    return true;
}

void vitals_compute(const float *red_bp, const float *ir_bp,
                     const uint32_t *red_raw, const uint32_t *ir_raw,
                     int n, vitals_result_t *out)
{
    out->hr = 0.0f;
    out->spo2 = 0.0f;
    out->hr_valid = false;
    out->spo2_valid = false;
    out->num_peaks = 0;
    out->finger_detected = false;

    /* Kiem tra DC trung binh cua kenh IR - xac dinh co ngon tay hay khong.
     * Neu khong dat, coi nhu khong co du lieu hop le, khong chay peak
     * detection (tranh sinh ra HR/SpO2 "ao" tu nhieu nen khi chua dat tay). */
    float dc_ir_sum = 0.0f;
    for (int i = 0; i < n; i++) {
        dc_ir_sum += (float)ir_raw[i];
    }
    float dc_ir_mean = dc_ir_sum / (float)n;

    if (dc_ir_mean < VITALS_MIN_IR_DC) {
        return;   // khong co ngon tay -> dung lai, moi gia tri van la false/0
    }
    out->finger_detected = true;

    int peak_idx[VITALS_MAX_PEAKS];
    int num_peaks = find_ir_peaks(ir_bp, n, peak_idx);
    out->num_peaks = num_peaks;

    out->hr_valid   = calc_hr(peak_idx, num_peaks, &out->hr);
    out->spo2_valid = calc_spo2(red_bp, ir_bp, red_raw, ir_raw, peak_idx, num_peaks, &out->spo2);
}