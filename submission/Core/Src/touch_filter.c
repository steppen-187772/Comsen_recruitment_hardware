#include "touch_filter.h"

/*
 * 按键消抖 / 去重
 * ============================================================================
 * 输入：ttp229_read_physical() 直接读回来的原始位图（一次按下可能出现好几位）。
 * 输出：每 10ms 调用一次，只在"新按下"的那一拍返回一个键位，其余返回 0。
 *
 * 三段逻辑：
 *   1) 计数消抖：每个键各有 0~THRESHOLD 的计数器，连续稳定若干拍才认为按下/松开；
 *   2) 稳定位图：stable_bitmap 记录"确认按下"的键（可能多位，交给触摸模型辨认）；
 *   3) 单次上报：一次按下只报一次，直到所有键都松开（wait_all_released）才允许再报。
 */

#ifndef TOUCH_FILTER_THRESHOLD
#define TOUCH_FILTER_THRESHOLD 3u   /* 连续 3 拍（约 30ms）一致才算稳定 */
#endif

void touch_filter_init(touch_filter_t *filter)
{
  uint8_t i;

  if (filter == 0)
  {
    return;
  }

  for (i = 0u; i < 30u; i++)
  {
    filter->counters[i] = 0u;
  }
  filter->stable_bitmap = 0u;
  filter->active_bitmap = 0u;
  filter->wait_all_released = 0u;
}

uint32_t touch_filter_update(touch_filter_t *filter, uint32_t raw_bitmap)
{
  uint32_t stable = 0u;
  uint32_t pressed_event = 0u;
  uint8_t i;

  if (filter == 0)
  {
    return 0u;
  }

  /* ---- 1) 每个键独立消抖 ---- */
  for (i = 0u; i < 30u; i++)
  {
    uint32_t mask = (uint32_t)1u << i;

    if ((raw_bitmap & mask) != 0u)
    {
      if (filter->counters[i] < TOUCH_FILTER_THRESHOLD)
      {
        filter->counters[i]++;
      }
    }
    else if (filter->counters[i] > 0u)
    {
      filter->counters[i]--;                 /* 松手也慢慢降，抗毛刺 */
    }

    if (filter->counters[i] >= TOUCH_FILTER_THRESHOLD)
    {
      stable |= mask;
    }
  }
  filter->stable_bitmap = stable;

  /* ---- 2) 全部松开：解锁，准备迎接下一次按键 ---- */
  if (stable == 0u)
  {
    filter->active_bitmap = 0u;
    filter->wait_all_released = 0u;
    return 0u;
  }

  /* ---- 3) 还没松完手，不重复上报 ---- */
  if (filter->wait_all_released != 0u)
  {
    return 0u;
  }

  /* 取最低位作为本次物理按键（真正的"用户想按哪个键"交给触摸模型判断） */
  i = 0u;
  while (((stable >> i) & 1u) == 0u)
  {
    i++;
  }
  pressed_event = (uint32_t)1u << i;
  filter->active_bitmap = pressed_event;
  filter->wait_all_released = 1u;

  return pressed_event;
}
