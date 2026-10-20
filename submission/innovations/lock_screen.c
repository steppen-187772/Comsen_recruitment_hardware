#include "lock_screen.h"
#include "lcd1602.h"
#include "cry_face.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/*
 * 数字雨锁屏
 * ============================================================================
 * - 8 个数字粒子，按奇偶分配到上下两行；
 * - 每个粒子从 0..14 列随机生成，向右移动；
 * - 到随机终点后消失并在随机位置重生；
 * - 速度：90 + 数字*22 + 随机 0..29 毫秒（数字越大越慢）。
 */

#define RAIN_PARTICLES 8u
#define MODE_KEY_BIT   (1u << 2)      /* T2 = MODE */

typedef struct
{
  uint8_t  digit;
  uint8_t  row;
  int8_t   x;
  int8_t   end_x;
  uint16_t speed_ms;
  uint32_t next_move;
} rain_t;

static rain_t   g_p[RAIN_PARTICLES];
static uint32_t g_rng = 0x9E3779B9u;
static uint8_t  g_locked = 1u;

static uint32_t rnd(void)
{
  g_rng = (g_rng * 1664525u) + 1013904223u;
  return g_rng;
}

static uint16_t speed_for(uint8_t digit)
{
  return (uint16_t)(90u + (digit * 22u) + (rnd() % 30u));
}

static void respawn(rain_t *p, uint8_t row, uint32_t now)
{
  uint8_t x0 = (uint8_t)(rnd() % 15u);
  uint8_t room = (uint8_t)(15u - x0);

  p->digit = (uint8_t)(rnd() % 10u);
  p->row = row;
  p->x = (int8_t)x0;
  p->end_x = (int8_t)(x0 + 1u + (rnd() % room));
  p->speed_ms = speed_for(p->digit);
  p->next_move = now + (rnd() % 200u);
}

void lock_screen_render(void)
{
  char l1[16];
  char l2[16];
  uint8_t i;

  memset(l1, ' ', sizeof(l1));
  memset(l2, ' ', sizeof(l2));

  for (i = 0u; i < RAIN_PARTICLES; i++)
  {
    rain_t *p = &g_p[i];

    if ((p->x >= 0) && (p->x <= 15))
    {
      ((p->row == 0u) ? l1 : l2)[p->x] = (char)('0' + p->digit);
    }
  }
  lcd1602_write_lines(l1, l2);
}

void lock_screen_init(void)
{
  uint32_t now;
  uint8_t i;

  g_rng ^= HAL_GetTick() + 0x1234567u;
  g_locked = 1u;
  now = HAL_GetTick();
  for (i = 0u; i < RAIN_PARTICLES; i++)
  {
    respawn(&g_p[i], (uint8_t)(i & 1u), now);
  }
}

uint8_t lock_screen_is_locked(void)
{
  return g_locked;
}

uint8_t lock_screen_handle_key(uint32_t key_edge, uint32_t stable_bitmap)
{
  uint8_t mode_in_set = ((stable_bitmap & MODE_KEY_BIT) != 0u) ? 1u : 0u;
  uint8_t clean_mode = (mode_in_set && (stable_bitmap == MODE_KEY_BIT)) ? 1u : 0u;

  (void)key_edge;

  if (g_locked != 0u)
  {
    if (mode_in_set != 0u)
    {
      char l1[16];
      char l2[16];

      g_locked = 0u;
      memset(l1, ' ', sizeof(l1));
      memset(l2, ' ', sizeof(l2));
      memcpy(&l2[11], "READY", 5u);
      lcd1602_write_lines(l1, l2);
      return 1u;
    }
    return 1u;                 /* 锁屏期间其它按键全部吞掉 */
  }

  if (clean_mode != 0u)
  {
    g_locked = 1u;
    lock_screen_render();
    return 1u;
  }
  return 0u;
}

void lock_screen_tick(uint32_t now_ms)
{
  uint8_t moved = 0u;
  uint8_t i;

  if (g_locked == 0u)
  {
    return;
  }
  for (i = 0u; i < RAIN_PARTICLES; i++)
  {
    rain_t *p = &g_p[i];

    if ((int32_t)(now_ms - p->next_move) >= 0)
    {
      p->x++;
      p->next_move = now_ms + p->speed_ms;
      if (p->x > p->end_x)
      {
        respawn(p, (uint8_t)(i & 1u), now_ms);
      }
      moved = 1u;
    }
  }
  if (moved != 0u)
  {
    lock_screen_render();
  }
}

uint8_t lock_screen_apply_error_face(char line[16])
{
  return cry_face_replace_error(line);
}
