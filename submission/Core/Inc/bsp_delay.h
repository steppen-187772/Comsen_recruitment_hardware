#ifndef BSP_DELAY_H
#define BSP_DELAY_H

#include <stdint.h>

/*
 * 微秒级延时 —— 触摸时序和液晶时序都要用。
 *
 * 前提：CubeMX 里把 TIM3 配成 72MHz/72 = 1MHz，并在 main() 里
 *       HAL_TIM_Base_Start(&htim3)，也就是"1 个计数 = 1 微秒"。
 *       这与示例工程 software.ioc 的配置一致（Prescaler=71）。
 */
void bsp_delay_us(uint32_t us);

/* 毫秒延时：直接借 HAL 的时基（示例里 TIM4 提供，也可以是 SysTick） */
void bsp_delay_ms(uint32_t ms);

#endif /* BSP_DELAY_H */
