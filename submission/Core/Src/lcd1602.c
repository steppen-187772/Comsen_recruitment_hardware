#include "lcd1602.h"
#include "board_config.h"
#include "bsp_delay.h"

/*
 * 1602 字符液晶驱动（HD44780 兼容，4 位模式）
 * ============================================================================
 * 接线（来自 Gerber，H3 排针）：
 *   RS = PB10   E = PB11   D4~D7 = PB12~PB15   RW 直接接 GND（只写模式）
 *   背光 A 经 R13 接 3.3V、K 接 GND —— 常亮，软件控制不了；
 *   对比度 V0 由 R11/R12 分压固定 —— 屏幕太淡/全黑是硬件问题，改电阻而不是改代码。
 *
 * 4 位模式的要点：一个字节分两次送，先高 4 位后低 4 位，每次都要给一个 EN 脉冲。
 */

/* ---------------- 底层：把 4 根数据线设成某个值 ---------------- */
static void lcd_put_nibble(uint8_t nibble)
{
  HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN,
                    ((nibble & 0x01u) != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN,
                    ((nibble & 0x02u) != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN,
                    ((nibble & 0x04u) != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN,
                    ((nibble & 0x08u) != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* 一个 EN 脉冲：数据已经摆好，高→低时液晶把数据读走 */
static void lcd_pulse_en(void)
{
  HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_SET);
  bsp_delay_us(LCD_EN_PULSE_US);
  HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);
  bsp_delay_us(LCD_CMD_SETTLE_US);
}

/* 送一个半字节（不区分 RS，RS 由调用者先设好） */
static void lcd_write_nibble_raw(uint8_t nibble)
{
  lcd_put_nibble(nibble);
  lcd_pulse_en();
}

/* 以 rs 身份写一个字节：RS=0 是命令，RS=1 是数据 */
static void lcd_write_byte(uint8_t value, uint8_t rs)
{
  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, rs ? GPIO_PIN_SET : GPIO_PIN_RESET);
  lcd_write_nibble_raw((uint8_t)(value >> 4));      /* 高 4 位 */
  lcd_write_nibble_raw((uint8_t)(value & 0x0Fu));   /* 低 4 位 */
}

static void lcd_cmd(uint8_t cmd)
{
  lcd_write_byte(cmd, 0u);
  if ((cmd == 0x01u) || (cmd == 0x02u))
  {
    bsp_delay_ms(LCD_CLEAR_MS);                     /* 清屏/回原点要等久一点 */
  }
}

static void lcd_data(uint8_t data)
{
  lcd_write_byte(data, 1u);
}

static void lcd_set_cursor(uint8_t row, uint8_t column)
{
  /* 第 0 行首地址 0x00，第 1 行首地址 0x40 */
  uint8_t addr = (uint8_t)((row == 0u) ? 0x00u : 0x40u);
  addr = (uint8_t)(addr + (column & 0x0Fu));
  lcd_cmd((uint8_t)(0x80u | addr));
}

/* ---------------- 对外接口 ---------------- */

void lcd1602_define_char(uint8_t slot, const uint8_t rows[8])
{
  uint8_t i;

  if (slot > 7u)
  {
    return;
  }
  lcd_cmd((uint8_t)(0x40u | (uint8_t)(slot << 3)));   /* CGRAM 地址 */
  for (i = 0u; i < 8u; i++)
  {
    lcd_data(rows[i]);                                /* 每行只要低 5 位有效 */
  }
  lcd_set_cursor(0u, 0u);                             /* 回到 DDRAM */
}

void lcd1602_init(void)
{
  /*
   * 上电后至少等 40ms，再按 HD44780 手册的"反复软复位"把芯片拉进 4 位模式。
   * 这一步不能省，否则经常出现"屏幕能亮但一个字都不显示"。
   */
  bsp_delay_ms(40u);

  HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_EN_PORT, LCD_EN_PIN, GPIO_PIN_RESET);

  lcd_write_nibble_raw(0x03u);
  bsp_delay_ms(5u);
  lcd_write_nibble_raw(0x03u);
  bsp_delay_ms(1u);
  lcd_write_nibble_raw(0x03u);
  bsp_delay_ms(1u);
  lcd_write_nibble_raw(0x02u);     /* 从这里开始进入 4 位模式 */
  bsp_delay_ms(1u);

  lcd_cmd(0x28u);                  /* 4 位、2 行、5x8 点阵 */
  lcd_cmd(0x0Cu);                  /* 开显示、关光标、不闪烁 */
  lcd_cmd(0x06u);                  /* 写入后光标右移，画面不滚动 */
  lcd_cmd(0x01u);                  /* 清屏 */

  /* 给"角度/极坐标"符号准备一个自定义字符，占用槽位 LCD1602_CHAR_ANGLE */
  {
    static const uint8_t angle_char[8] =
    {
      0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x1Fu, 0x00u, 0x00u   /* 一个 ∠ 形状 */
    };
    lcd1602_define_char(LCD1602_CHAR_ANGLE, angle_char);
  }

  lcd_cmd(0x01u);
}

void lcd1602_write_lines(const char line1[16], const char line2[16])
{
  uint8_t i;

  lcd_set_cursor(0u, 0u);
  for (i = 0u; i < 16u; i++)
  {
    lcd_data((uint8_t)line1[i]);
  }
  lcd_set_cursor(1u, 0u);
  for (i = 0u; i < 16u; i++)
  {
    lcd_data((uint8_t)line2[i]);
  }
}

void lcd1602_write_frame(const char line1[16], const char line2[16],
                         uint8_t cursor_enabled, uint8_t cursor_row,
                         uint8_t cursor_column)
{
  lcd1602_write_lines(line1, line2);

  if (cursor_enabled != 0u)
  {
    lcd_cmd(0x0Fu);                            /* 开显示 + 下划线光标 + 闪烁 */
    lcd_set_cursor((uint8_t)(cursor_row != 0u ? 1u : 0u),
                   (uint8_t)(cursor_column % 16u));
  }
  else
  {
    lcd_cmd(0x0Cu);                            /* 只要显示 */
  }
}
