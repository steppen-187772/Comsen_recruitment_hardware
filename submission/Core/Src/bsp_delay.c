#include "bsp_delay.h"
#include "stm32f1xx_hal.h"

/* htim3 在 main.c 里定义，CubeMX 生成的 main.h 也会 extern 声明它 */
extern TIM_HandleTypeDef htim3;

void bsp_delay_us(uint32_t us)
{
  uint32_t start;
  uint32_t now;

  if (us == 0u)
  {
    return;
  }

  /*
   * TIM3 向上计数、1MHz，每 65536us 回绕一次。
   * 只比较"经过了多少个计数"，即使中途回绕也仍然正确，
   * 因此这里用"目标差值"而不是"比较绝对值"。
   */
  start = __HAL_TIM_GET_COUNTER(&htim3) & 0xFFFFu;
  do
  {
    now = __HAL_TIM_GET_COUNTER(&htim3) & 0xFFFFu;
  } while (((now - start) & 0xFFFFu) < us);
}

void bsp_delay_ms(uint32_t ms)
{
  HAL_Delay(ms);
}
