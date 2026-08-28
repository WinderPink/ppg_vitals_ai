#include "ssd1306.h"
#include "ssd1306_font.h"

/* Control byte: bit Co=0 (nhiều byte theo sau cùng loại), bit D/C# xác định
 * byte sau là command (0x00) hay data (0x40) */
#define SSD1306_CTRL_CMD    0x00
#define SSD1306_CTRL_DATA   0x40

static bool ssd1306_send(sl_i2cspm_t *i2cspm, uint8_t ctrl, const uint8_t *buf, uint16_t len)
{
  I2C_TransferSeq_TypeDef seq;
  uint8_t ctrl_byte = ctrl;

  seq.addr    = SSD1306_I2C_ADDR << 1;
  seq.flags   = I2C_FLAG_WRITE_WRITE; // 2 buffer ghi liên tiếp, không STOP giữa chừng
  seq.buf[0].data = &ctrl_byte;
  seq.buf[0].len  = 1;
  seq.buf[1].data = (uint8_t *)buf;
  seq.buf[1].len  = len;

  return (I2CSPM_Transfer(i2cspm, &seq) == i2cTransferDone);
}

static bool ssd1306_write_cmd(sl_i2cspm_t *i2cspm, uint8_t cmd)
{
  return ssd1306_send(i2cspm, SSD1306_CTRL_CMD, &cmd, 1);
}

bool ssd1306_init(sl_i2cspm_t *i2cspm)
{
  bool ok = true;

  ok &= ssd1306_write_cmd(i2cspm, 0xAE); // display off
  ok &= ssd1306_write_cmd(i2cspm, 0xD5); // clock divide ratio/osc freq
  ok &= ssd1306_write_cmd(i2cspm, 0x80);
  ok &= ssd1306_write_cmd(i2cspm, 0xA8); // multiplex ratio
  ok &= ssd1306_write_cmd(i2cspm, 0x3F); // 64-1
  ok &= ssd1306_write_cmd(i2cspm, 0xD3); // display offset
  ok &= ssd1306_write_cmd(i2cspm, 0x00);
  ok &= ssd1306_write_cmd(i2cspm, 0x40); // start line = 0
  ok &= ssd1306_write_cmd(i2cspm, 0x8D); // charge pump
  ok &= ssd1306_write_cmd(i2cspm, 0x14); // enable
  ok &= ssd1306_write_cmd(i2cspm, 0x20); // memory addressing mode
  ok &= ssd1306_write_cmd(i2cspm, 0x02); // page addressing mode
  ok &= ssd1306_write_cmd(i2cspm, 0xA1); // segment remap (cột 127 = SEG0)
  ok &= ssd1306_write_cmd(i2cspm, 0xC8); // COM scan direction remapped
  ok &= ssd1306_write_cmd(i2cspm, 0xDA); // COM pins config
  ok &= ssd1306_write_cmd(i2cspm, 0x12);
  ok &= ssd1306_write_cmd(i2cspm, 0x81); // contrast
  ok &= ssd1306_write_cmd(i2cspm, 0xCF);
  ok &= ssd1306_write_cmd(i2cspm, 0xD9); // precharge period
  ok &= ssd1306_write_cmd(i2cspm, 0xF1);
  ok &= ssd1306_write_cmd(i2cspm, 0xDB); // VCOMH deselect level
  ok &= ssd1306_write_cmd(i2cspm, 0x40);
  ok &= ssd1306_write_cmd(i2cspm, 0xA4); // resume RAM content (không phải toàn sáng/toàn tắt)
  ok &= ssd1306_write_cmd(i2cspm, 0xA6); // normal display (không đảo màu)
  ok &= ssd1306_write_cmd(i2cspm, 0xAF); // display ON

  if (ok) {
    ssd1306_clear(i2cspm);
  }
  return ok;
}

void ssd1306_set_cursor(sl_i2cspm_t *i2cspm, uint8_t page, uint8_t col)
{
  if (page >= SSD1306_PAGES) page = SSD1306_PAGES - 1;
  if (col >= SSD1306_WIDTH)  col  = SSD1306_WIDTH - 1;

  ssd1306_write_cmd(i2cspm, 0xB0 | page);              // set page start address
  ssd1306_write_cmd(i2cspm, 0x00 | (col & 0x0F));       // lower nibble of column
  ssd1306_write_cmd(i2cspm, 0x10 | ((col >> 4) & 0x0F)); // higher nibble of column
}

void ssd1306_clear(sl_i2cspm_t *i2cspm)
{
  uint8_t blank_row[SSD1306_WIDTH] = {0};

  for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
    ssd1306_set_cursor(i2cspm, page, 0);
    ssd1306_send(i2cspm, SSD1306_CTRL_DATA, blank_row, SSD1306_WIDTH);
  }
  ssd1306_set_cursor(i2cspm, 0, 0);
}

void ssd1306_write_string(sl_i2cspm_t *i2cspm, const char *str)
{
  uint8_t glyph[6]; // 5 cột ký tự + 1 cột trắng để cách chữ

  while (*str) {
    ssd1306_font_lookup(*str, glyph);
    glyph[5] = 0x00; // cột đệm giữa các ký tự
    ssd1306_send(i2cspm, SSD1306_CTRL_DATA, glyph, 6);
    str++;
  }
}

void ssd1306_print_line(sl_i2cspm_t *i2cspm, uint8_t page, const char *str)
{
  uint8_t blank_row[SSD1306_WIDTH] = {0};

  ssd1306_set_cursor(i2cspm, page, 0);
  ssd1306_send(i2cspm, SSD1306_CTRL_DATA, blank_row, SSD1306_WIDTH); // xoá dòng cũ

  ssd1306_set_cursor(i2cspm, page, 0);
  ssd1306_write_string(i2cspm, str);
}