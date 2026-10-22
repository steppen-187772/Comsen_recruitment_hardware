#ifndef CALCULATOR_ENGINE_H
#define CALCULATOR_ENGINE_H

#include <stdint.h>

typedef enum
{
  CALC_ANGLE_DEG = 0,
  CALC_ANGLE_RAD
} calc_angle_unit_t;

typedef struct
{
  float real;
  float imag;
} calc_complex_t;

typedef enum
{
  CALC_OK = 0,
  CALC_SYNTAX,
  CALC_DOMAIN,
  CALC_DIV_ZERO
} calc_status_t;

calc_status_t calculator_evaluate(const char *expression, calc_angle_unit_t angle_unit,
                                  uint8_t allow_complex, calc_complex_t answer,
                                  calc_complex_t *result);

calc_status_t calculator_solve_linear(float a, float b, float *x);

/* ---------------- 替换实现额外提供的辅助接口 ---------------- */

/*
 * 把结果格式化成能直接上 1602 的字符串（不使用 printf/浮点打印，省 Flash）。
 * max_chars 是可用字符数（1602 一行最多 16）。返回实际长度。
 */
uint8_t calculator_format(const calc_complex_t *value, uint8_t max_chars, char *out);

#endif /* CALCULATOR_ENGINE_H */
