#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>

#define LCD1602_CHAR_ANGLE 1U

void lcd1602_init(void);
void lcd1602_write_lines(const char line1[16], const char line2[16]);
void lcd1602_write_frame(const char line1[16], const char line2[16], uint8_t cursor_enabled,
                         uint8_t cursor_row, uint8_t cursor_column);

/* ---------------- 替换实现额外提供的辅助接口 ---------------- */

/* 往 CGRAM 写一个 5x8 自定义字符（slot 0~7） */
void lcd1602_define_char(uint8_t slot, const uint8_t rows[8]);

#endif /* LCD1602_H */
