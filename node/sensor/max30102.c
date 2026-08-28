#include "max30102.h"
#include <string.h>
#include "app_log.h"

/* ---------------------------------------------------------------------------
 * Hàm nội bộ: thực hiện 1 transfer I2C dùng I2CSPM_Transfer (blocking)
 * ------------------------------------------------------------------------- */
static bool i2c_write(sl_i2cspm_t *i2cspm, uint8_t reg, const uint8_t *data, uint16_t len)
{
  I2C_TransferSeq_TypeDef seq;
  uint8_t write_buf[16]; // reg + tối đa 15 byte data

  if (len > sizeof(write_buf) - 1) {
    return false;
  }

  write_buf[0] = reg;
  memcpy(&write_buf[1], data, len);

  seq.addr    = MAX30102_I2C_ADDR << 1;
  seq.flags   = I2C_FLAG_WRITE;
  seq.buf[0].data = write_buf;
  seq.buf[0].len  = len + 1;
  seq.buf[1].data = NULL;
  seq.buf[1].len  = 0;

  return (I2CSPM_Transfer(i2cspm, &seq) == i2cTransferDone);
}

static bool i2c_read(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t *data, uint16_t len)
{
  I2C_TransferSeq_TypeDef seq;

  seq.addr    = MAX30102_I2C_ADDR << 1;
  seq.flags   = I2C_FLAG_WRITE_READ; // ghi địa chỉ thanh ghi rồi đọc lại
  seq.buf[0].data = &reg;
  seq.buf[0].len  = 1;
  seq.buf[1].data = data;
  seq.buf[1].len  = len;

  I2C_TransferReturn_TypeDef ret = I2CSPM_Transfer(i2cspm, &seq);
  if (ret != i2cTransferDone) {
    // In mã lỗi I2C (ví dụ: -1 = i2cTransferNack, -2 = i2cTransferBusErr, ...)
    app_log_error("[I2C Read Error] Reg: 0x%02X, Error Code: %d\r\n", reg, ret);
    return false;
  }
  return true;
}

bool max30102_write_reg(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t value)
{
  return i2c_write(i2cspm, reg, &value, 1);
}

bool max30102_read_reg(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t *value)
{
  return i2c_read(i2cspm, reg, value, 1);
}

bool max30102_read_regs(sl_i2cspm_t *i2cspm, uint8_t reg, uint8_t *buf, uint16_t len)
{
  return i2c_read(i2cspm, reg, buf, len);
}

bool max30102_init(sl_i2cspm_t *i2cspm)
{
  uint8_t part_id = 0;

  app_log_info("\r\n--- BẮT ĐẦU KHỞI TẠO MAX30102 ---\r\n");

  // 1. Kiểm tra kết nối bằng PART_ID
  if (!max30102_read_reg(i2cspm, MAX30102_REG_PART_ID, &part_id)) {
    app_log_error("[MAX30102] LOI: Khong the doc thanh ghi PART_ID qua I2C!\r\n");
    return false;
  }
  
  app_log_info("[MAX30102] Doc PART_ID = 0x%02X (Ky vong: 0x%02X)\r\n", part_id, MAX30102_EXPECTED_PART_ID);
  
  if (part_id != MAX30102_EXPECTED_PART_ID) {
    app_log_error("[MAX30102] LOI: PART_ID khong khop!\r\n");
    return false; 
  }

  // 2. Reset toàn bộ thanh ghi
  app_log_info("[MAX30102] Dang khoi dong lai (Soft Reset)...\r\n");
  if (!max30102_write_reg(i2cspm, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET)) {
    app_log_error("[MAX30102] LOI: Khong ghi duoc lenh Reset!\r\n");
    return false;
  }

  // Chờ bit RESET tự xoá về 0
  uint8_t mode = 0;
  uint32_t timeout = 10000;
  do {
    max30102_read_reg(i2cspm, MAX30102_REG_MODE_CONFIG, &mode);
  } while ((mode & MAX30102_MODE_RESET) && --timeout);

  if (timeout == 0) {
    app_log_error("[MAX30102] LOI: Timeout khi cho Soft Reset!\r\n");
    return false;
  }
  app_log_info("[MAX30102] Soft Reset thanh cong.\r\n");

  // 3. Cấu hình FIFO (0x4F: SMP_AVE=1, ROLLOVER=1, FIFO_A_FULL=0)
  if (!max30102_write_reg(i2cspm, MAX30102_REG_FIFO_CONFIG, 0x10)) {
    app_log_error("[MAX30102] LOI: Ghi FIFO_CONFIG thất bại!\r\n");
    return false;
  }

  // 4. Chọn chế độ SpO2
  if (!max30102_write_reg(i2cspm, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_SPO2)) {
    app_log_error("[MAX30102] LOI: Ghi MODE_CONFIG thất bại!\r\n");
    return false;
  }

  // 5. Cấu hình SpO2 (0x27)
  if (!max30102_write_reg(i2cspm, MAX30102_REG_SPO2_CONFIG, 0x27)) {
    app_log_error("[MAX30102] LOI: Ghi SPO2_CONFIG thất bại!\r\n");
    return false;
  }

  // 6. Dòng LED (Pulse Amplitude)
  if (!max30102_write_reg(i2cspm, MAX30102_REG_LED1_PA, 0x24) ||
      !max30102_write_reg(i2cspm, MAX30102_REG_LED2_PA, 0x24)) {
    app_log_error("[MAX30102] LOI: Ghi dong LED (PA) thất bại!\r\n");
    return false;
  }

  // 7. Reset con trỏ FIFO về 0
  max30102_write_reg(i2cspm, MAX30102_REG_FIFO_WR_PTR, 0x00);
  max30102_write_reg(i2cspm, MAX30102_REG_OVF_COUNTER, 0x00);
  max30102_write_reg(i2cspm, MAX30102_REG_FIFO_RD_PTR, 0x00);

  if (!max30102_write_reg(i2cspm, MAX30102_REG_INT_ENABLE1, MAX30102_INT_PPG_RDY_EN)) {
    app_log_error("[MAX30102] LOI: Bat ngat that bai!\r\n");
    return false;
  }
  app_log_info("[MAX30102] Da bat PPG_RDY (INT_ENABLE1 = 0x%02X)\r\n", MAX30102_INT_PPG_RDY_EN);
 
  /* Đọc (và nhờ đó xoá) mọi ngắt "tồn đọng" từ trước khi init, để đảm bảo
   * chân INT bắt đầu ở trạng thái mức cao trước khi MCU bật GPIO IRQ*/
  max30102_clear_interrupt(i2cspm);

  // 8. ĐỌC LẠI BẢNG CẤU HÌNH ĐỂ KIỂM TRA (READ-BACK CHECK)
  uint8_t check_fifo = 0, check_mode = 0, check_spo2 = 0, check_led1 = 0, check_led2 = 0;
  max30102_read_reg(i2cspm, MAX30102_REG_FIFO_CONFIG, &check_fifo);
  max30102_read_reg(i2cspm, MAX30102_REG_MODE_CONFIG, &check_mode);
  max30102_read_reg(i2cspm, MAX30102_REG_SPO2_CONFIG, &check_spo2);
  max30102_read_reg(i2cspm, MAX30102_REG_LED1_PA, &check_led1);
  max30102_read_reg(i2cspm, MAX30102_REG_LED2_PA, &check_led2);

  app_log_info("[MAX30102 Check] FIFO_CFG: 0x%02X (Mong muon: 0x10)\r\n", check_fifo);
  app_log_info("[MAX30102 Check] MODE_CFG: 0x%02X (Mong muon: 0x03)\r\n", check_mode);
  app_log_info("[MAX30102 Check] SPO2_CFG: 0x%02X (Mong muon: 0x27)\r\n", check_spo2);
  app_log_info("[MAX30102 Check] LED1_PA : 0x%02X | LED2_PA: 0x%02X\r\n", check_led1, check_led2);

  app_log_info("--- KHOI TAO MAX30102 HOAN TAT THANH CONG ---\r\n\r\n");
  return true;
}

uint8_t max30102_get_fifo_count(sl_i2cspm_t *i2cspm)
{
  uint8_t ptrs[3] = {0}; // ptrs[0]: WR_PTR, ptrs[1]: OVF_COUNTER, ptrs[2]: RD_PTR

  // Đọc liền 3 byte con trỏ FIFO (từ địa chỉ 0x04 đến 0x06)
  if (!max30102_read_regs(i2cspm, MAX30102_REG_FIFO_WR_PTR, ptrs, 3)) {
    app_log_error("[MAX30102] Loi doc I2C con tro FIFO!\r\n");
    return 0;
  }

  uint8_t wr_ptr = ptrs[0] & 0x1F; // 5-bit (0 - 31)
  uint8_t rd_ptr = ptrs[2] & 0x1F; // 5-bit (0 - 31)

  int16_t count = (int16_t)wr_ptr - (int16_t)rd_ptr;
  if (count < 0) {
    count += 32;
  }

  // static uint16_t debug_cnt = 0;
  // if (++debug_cnt % 5 == 0) { // Cứ 1 giây in 1 lần log nhịp tim
  //   app_log_info("[FIFO Status] WR_PTR=%d, RD_PTR=%d -> Samples=%d\r\n", wr_ptr, rd_ptr, count);
  // }

  return (uint8_t)count;
}

bool max30102_read_fifo_sample(sl_i2cspm_t *i2cspm, max30102_sample_t *sample)
{
  uint8_t raw[6]; // 3 byte RED + 3 byte IR

  if (!max30102_read_regs(i2cspm, MAX30102_REG_FIFO_DATA, raw, 6)) {
    app_log_error("[MAX30102] LOI: Khong doc duoc FIFO DATA!\r\n");
    return false;
  }

  sample->red = (((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2]) & 0x3FFFF;
  sample->ir  = (((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | raw[5]) & 0x3FFFF;

  return true;
}

uint8_t max30102_read_fifo_burst(sl_i2cspm_t *i2cspm, max30102_sample_t *samples, uint8_t count)
{
  for (uint8_t i = 0; i < count; i++) {
    if (!max30102_read_fifo_sample(i2cspm, &samples[i])) {
      return i; // Trả về số lượng đã đọc được trước khi bị lỗi
    }
  }
  return count;
}

void max30102_shutdown(sl_i2cspm_t *i2cspm, bool enable)
{
  uint8_t mode = 0;
  max30102_read_reg(i2cspm, MAX30102_REG_MODE_CONFIG, &mode);

  if (enable) {
    mode |= MAX30102_MODE_SHUTDOWN;
  } else {
    mode &= ~MAX30102_MODE_SHUTDOWN;
  }
  max30102_write_reg(i2cspm, MAX30102_REG_MODE_CONFIG, mode);
}

/* Hàm xoá ngắt trên chip MAX30102
 * Đọc thanh ghi INT_STATUS1 (và INT_STATUS2) sẽ tự động xoá
 * toàn bộ cờ ngắt đang treo, đồng thời thả chân INT về mức cao trở lại. */
bool max30102_clear_interrupt(sl_i2cspm_t *i2cspm)
{
  uint8_t status1 = 0, status2 = 0;
  bool ok1 = max30102_read_reg(i2cspm, MAX30102_REG_INT_STATUS1, &status1);
  bool ok2 = max30102_read_reg(i2cspm, MAX30102_REG_INT_STATUS2, &status2);
 
  if (!ok1 || !ok2) {
    app_log_error("[MAX30102] LOI: Khong doc duoc INT_STATUS de xoa ngat!\r\n");
    return false;
  }
  return true;
}

