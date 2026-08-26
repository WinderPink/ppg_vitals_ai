#include "app_ml.h"
#include "vitals.h"
#include "sl_status.h"
#include "sl_ml_model_bp_model_int8.h"   
#include "sl_ml_tflite_micro_model.h"
#include <string.h>
#include <math.h>
#include "app_log.h"

#define PPG_WINDOW_SIZE   800   // 8s @ 100Hz - phai khop VITALS_WINDOW_SAMPLES va input model
#define INFERENCE_HOP     100   // chay lai HR/SpO2 + AI moi 100 mau moi (~1s)

/* ---- Circular buffer cho ca vitals va AI, dung chung 1 window ---- */
static float    cb_red_bp[PPG_WINDOW_SIZE];
static float    cb_ir_bp[PPG_WINDOW_SIZE];
static float    cb_red_z[PPG_WINDOW_SIZE];
static float    cb_ir_z[PPG_WINDOW_SIZE];
static uint32_t cb_red_raw[PPG_WINDOW_SIZE];
static uint32_t cb_ir_raw[PPG_WINDOW_SIZE];

static int cb_write_idx = 0;     // vi tri ghi tiep theo (circular)
static int cb_fill_count = 0;    // so mau da co (toi da PPG_WINDOW_SIZE)
static int samples_since_run = 0;

/* Buffer "duoi thang" tam thoi de dua vao vitals_compute()/model (yeu cau thu tu thoi gian) */
static float    lin_red_bp[PPG_WINDOW_SIZE];
static float    lin_ir_bp[PPG_WINDOW_SIZE];
static float    lin_red_z[PPG_WINDOW_SIZE];
static float    lin_ir_z[PPG_WINDOW_SIZE];
static uint32_t lin_red_raw[PPG_WINDOW_SIZE];
static uint32_t lin_ir_raw[PPG_WINDOW_SIZE];

QueueHandle_t xAIResultQueue = NULL;

extern "C" void ai_result_queue_create(void)
{
    xAIResultQueue = xQueueCreate(4, sizeof(ai_result_t));
}

extern "C" void ml_init(void)
{
    sl_status_t status = sl_ml_model_init(&sl_ml_bp_model_int8_model_handle); 
    if (status != SL_STATUS_OK) {
        app_log_error("[ML] Loi khoi tao model! status=%d\r\n", (int)status);
        return;
    }
    app_log_info("[ML] Model da khoi tao thanh cong. Dat ngon tay vao cam bien...\r\n");
    ml_reset();
}
extern "C" void ml_reset(void)
{
    cb_write_idx = 0;
    cb_fill_count = 0;
    samples_since_run = 0;
}

/* Doc circular buffer ra dang tuyen tinh, dung (oldest -> newest) */
static void unwrap_buffers(void)
{
    // Vi buffer da day (cb_fill_count == PPG_WINDOW_SIZE), phan tu CU NHAT
    // chinh la phan tu ngay sau vi tri ghi tiep theo (cb_write_idx).
    int start = cb_write_idx;   // vi cb_write_idx da wrap, day chinh la oldest
    for (int i = 0; i < PPG_WINDOW_SIZE; i++) {
        int src = (start + i) % PPG_WINDOW_SIZE;
        lin_red_bp[i]  = cb_red_bp[src];
        lin_ir_bp[i]   = cb_ir_bp[src];
        lin_red_z[i]   = cb_red_z[src];
        lin_ir_z[i]    = cb_ir_z[src];
        lin_red_raw[i] = cb_red_raw[src];
        lin_ir_raw[i]  = cb_ir_raw[src];
    }
}

static inline int8_t quantize_i8(float x, float scale, int32_t zero_point)
{
    int32_t q = (int32_t)lroundf(x / scale) + zero_point;
    if (q < -128) q = -128;
    if (q > 127)  q = 127;
    return (int8_t)q;
}

static inline float dequantize_i8(int8_t q, float scale, int32_t zero_point)
{
    return (float)((int32_t)q - zero_point) * scale;
}

/* Chay ca vitals (cong thuc) VA AI (model), in gop 1 dong duy nhat */
static void run_vitals_and_ai(void)
{
    unwrap_buffers();

    /* ---- 1. HR/SpO2 bang cong thuc, tren tin hieu bandpass-only + raw ---- */
    vitals_result_t vit;
    vitals_compute(lin_red_bp, lin_ir_bp, lin_red_raw, lin_ir_raw,
                    PPG_WINDOW_SIZE, &vit);

    if (!vit.finger_detected) {
        app_log_info("[RESULT] Khong phat hien ngon tay\r\n");
        ai_result_t res = {};
        if (xAIResultQueue) xQueueSend(xAIResultQueue, &res, 0);
        return;
    }

    /* ---- 2. SBP/DBP bang AI, chi chay khi HR/SpO2 hop le ---- */
    float sbp = 0.0f, dbp = 0.0f;
    bool bp_ok = false;

    TfLiteTensor *wave_t = sl_ml_bp_model_int8_model_handle.input_tensor(0);

    if (wave_t != NULL && wave_t->dims->size == 3) {
        float w_scale = wave_t->params.scale;
        int32_t w_zp  = wave_t->params.zero_point;

        // Interleave [red_z[t], ir_z[t]] vao input tensor
        for (int t = 0; t < PPG_WINDOW_SIZE; t++) {
            wave_t->data.int8[t * 2 + 0] = quantize_i8(lin_red_z[t], w_scale, w_zp);
            wave_t->data.int8[t * 2 + 1] = quantize_i8(lin_ir_z[t],  w_scale, w_zp);
        }

        sl_status_t status = sl_ml_model_run(&sl_ml_bp_model_int8_model_handle);
        if (status != SL_STATUS_OK) {
            app_log_error("[ML] Loi khi chay inference! status=%d\r\n", (int)status);
        } else {
            TfLiteTensor *output = sl_ml_bp_model_int8_model_handle.output_tensor(0);
            float out_scale = output->params.scale;
            int32_t out_zp  = output->params.zero_point;
            
            sbp = dequantize_i8(output->data.int8[0], out_scale, out_zp);
            dbp = dequantize_i8(output->data.int8[1], out_scale, out_zp);
            bp_ok = true;
        }
    } else {
        app_log_error("[ML] Input tensor khong dung dinh dang [1, 800, 2]!\r\n");
    }

    ai_result_t res = {};
    res.finger_detected = vit.finger_detected;
    res.hr = vit.hr;     res.hr_valid = vit.hr_valid;
    res.spo2 = vit.spo2; res.spo2_valid = vit.spo2_valid;
    res.sbp = sbp;       res.dbp = dbp;   res.bp_valid = bp_ok;

    if (xAIResultQueue) {
        xQueueSend(xAIResultQueue, &res, 0);
    }
}

extern "C" void ml_push_sample(float red_bp, float ir_bp,
                                float red_z,  float ir_z,
                                uint32_t red_raw, uint32_t ir_raw)
{
    cb_red_bp[cb_write_idx]  = red_bp;
    cb_ir_bp[cb_write_idx]   = ir_bp;
    cb_red_z[cb_write_idx]   = red_z;
    cb_ir_z[cb_write_idx]    = ir_z;
    cb_red_raw[cb_write_idx] = red_raw;
    cb_ir_raw[cb_write_idx]  = ir_raw;

    cb_write_idx = (cb_write_idx + 1) % PPG_WINDOW_SIZE;
    if (cb_fill_count < PPG_WINDOW_SIZE) {
        cb_fill_count++;
        if (cb_fill_count == PPG_WINDOW_SIZE) {
            app_log_info("[ML] Da du 8s du lieu, bat dau tinh toan...\r\n");
        }
    }

    if (cb_fill_count == PPG_WINDOW_SIZE) {
        samples_since_run++;
        if (samples_since_run >= INFERENCE_HOP) {
            run_vitals_and_ai();
            samples_since_run = 0;
        }
    }
}