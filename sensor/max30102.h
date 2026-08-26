#ifndef MAX30102_H
#define MAX30102_H

#include <stdint.h>
#include <stdbool.h>
#include "sl_i2cspm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX30102_I2C_ADDR          0x57   // 7-bit address

/* (Register Map) */
#define MAX30102_REG_INT_STATUS1   0x00
#define MAX30102_REG_INT_STATUS2   0x01
#define MAX30102_REG_INT_ENABLE1   0x02
#define MAX30102_REG_INT_ENABLE2   0x03
#define MAX30102_REG_FIFO_WR_PTR   0x04
#define MAX30102_REG_OVF_COUNTER   0x05
#define MAX30102_REG_FIFO_RD_PTR   0x06
#define MAX30102_REG_FIFO_DATA     0x07
#define MAX30102_REG_FIFO_CONFIG   0x08
#define MAX30102_REG_MODE_CONFIG   0x09
#define MAX30102_REG_SPO2_CONFIG   0x0A
#define MAX30102_REG_LED1_PA       0x0C   // RED LED
#define MAX30102_REG_LED2_PA       0x0D   // IR LED
#define MAX30102_REG_TEMP_INT      0x1F
#define MAX30102_REG_TEMP_FRAC     0x20
#define MAX30102_REG_TEMP_CONFIG   0x21
#define MAX30102_REG_REV_ID        0xFE
#define MAX30102_REG_PART_ID       0xFF

#define MAX30102_EXPECTED_PART_ID  0x15

#define MAX30102_MODE_HR_ONLY      0x02   // chỉ RED
#define MAX30102_MODE_SPO2         0x03   // RED + IR
#define MAX30102_MODE_MULTI_LED    0x07
#define MAX30102_MODE_RESET        0x40
#define MAX30102_MODE_SHUTDOWN     0x80

#define MAX30102_INT_A_FULL_EN     0X80   // ngắt khi FIFO đạt ngưỡng (FIFO_FULL)
#define MAX30102_INT_PPG_RDY_EN   0X40   // ngắt khi có mẫu mới (DATA_RDY)

typedef struct {
  uint32_t red;
  uint32_t ir;
} max30102_sample_t;


/**
 * @brief  Khởi tạo cảm biến MAX30102 (reset + cấu hình FIFO/SpO2/LED)
 * @param  i2cspm  con trỏ tới instance I2CSPM 
 * @return true nếu khởi tạo thành công (đọc đúng PART_ID)
 */
bool max30102_init(sl_i2cspm_t *i2cspm);

/**
 * @brief  Ghi 1 byte vào thanh ghi
 */
bool max30102_write_reg(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t value);

/**
 * @brief  Đọc 1 byte từ thanh ghi
 */
bool max30102_read_reg(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t *value);

/**
 * @brief  Đọc nhiều byte liên tiếp bắt đầu từ 1 thanh ghi
 */
bool max30102_read_regs(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t *buf, uint16_t len);

/**
 * @brief  Kiểm tra số mẫu hiện có trong FIFO (dựa trên WR_PTR/RD_PTR)
 */
uint8_t max30102_get_fifo_count(sl_i2cspm_t *i2cspm);

/**
 * @brief  Đọc 1 mẫu (RED + IR, mỗi giá trị 18-bit) từ FIFO
 * @return true nếu đọc thành công
 */
bool max30102_read_fifo_sample(sl_i2cspm_t *i2cspm, max30102_sample_t *sample);

/**
 * @brief  Đọc nhiều mẫu cùng lúc từ FIFO (tối ưu hơn đọc từng mẫu)
 * @param  samples  mảng chứa kết quả
 * @param  count    số mẫu muốn đọc
 * @return số mẫu thực sự đọc được
 */
uint8_t max30102_read_fifo_burst(sl_i2cspm_t *i2cspm, max30102_sample_t *samples, uint8_t count);

/**
 * @brief  Đưa cảm biến vào chế độ shutdown (tiết kiệm điện)
 */
void max30102_shutdown(sl_i2cspm_t *i2cspm, bool enable);

bool max30102_clear_interrupt(sl_i2cspm_t *i2cspm);

#ifdef __cplusplus
}
#endif

#endif // MAX30102_H