#ifndef LOCK_SCREEN_H
#define LOCK_SCREEN_H

#include <stdint.h>

/*
 * 创新点一：数字雨锁屏（默认锁屏，按 MODE 解锁进计算器）。
 *
 * 依赖：
 *   - lcd1602_write_lines()：刷新两行 16 字符；
 *   - HAL_GetTick()：初始化随机种子与时间基准。
 *
 * 建议在 10ms 的按键扫描节拍里调用 lock_screen_tick(now)。
 */

void    lock_screen_init(void);
uint8_t lock_screen_is_locked(void);

/*
 * 处理一个按键边沿；返回 1 表示被锁屏消费，不能再传给计算器。
 * 锁屏中：观测到 MODE(T2) 即解锁；
 * 计算中：只有"干净的单键 MODE"（stable_bitmap 只有 T2）才重新锁屏，
 *          避免按 ←（T7）时 MODE 串扰一起触发而误锁。
 */
uint8_t lock_screen_handle_key(uint32_t key_edge, uint32_t stable_bitmap);

void    lock_screen_tick(uint32_t now_ms);
void    lock_screen_render(void);

/* 兼容接口：内部转调用 cry_face_replace_error()，方便旧代码直接替换 */
uint8_t lock_screen_apply_error_face(char line[16]);

#endif /* LOCK_SCREEN_H */
