/***************************************************************************//**
 * @file app_health_data.h
 * @brief Cấu trúc dữ liệu sức khỏe dùng chung giữa module quét/giải mã EAD
 *        (app_ead_scanner.c) và module hiển thị OLED (app_oled.c).
 ******************************************************************************/
#ifndef APP_HEALTH_DATA_H
#define APP_HEALTH_DATA_H

#include <stdint.h>

// -----------------------------------------------------------------------------
// Custom Health Service Payload Structure (Must match Advertiser)
// -----------------------------------------------------------------------------
typedef struct {
  uint8_t heart_rate;   // Nhịp tim (bpm)
  uint8_t systolic;     // Huyết áp tâm thu (mmHg)
  uint8_t diastolic;    // Huyết áp tâm trương (mmHg)
  uint8_t spo2;         // Nồng độ oxy trong máu (%)
} health_data_t;

#endif // APP_HEALTH_DATA_H
