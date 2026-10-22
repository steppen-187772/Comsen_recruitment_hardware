#include "calculator_engine.h"
#include <math.h>

/*
 * 表达式计算引擎（替换 libcalculator_engine.a）
 * ============================================================================
 * 采用"递归下降 + 直接求值"，不建语法树，省 RAM：
 *
 *   表达式 := 项 { ('+'|'-') 项 }
 *   项     := 一元 { ('*'|'/') 一元 | 隐式乘法 }
 *   一元   := ('+'|'-') 一元 | 幂
 *   幂     := 主值 [ '^' 一元 ]           （右结合，允许 2^-3）
 *   主值   := 数字 | '(' 表达式 ')' | 函数 '(' 表达式 ')' | 常数 | 极坐标
 *   极坐标 := 表达式 '∠' 表达式           （a∠b = a·(cos b + i·sin b)）
 *
 * 支持的函数：sin cos tan asin acos atan sinh cosh tanh ln log exp sqrt abs
 *             arg conj re im
 * 支持的常数：pi / π, e, i（虚数单位）
 * 角度单位：CALC_ANGLE_DEG / CALC_ANGLE_RAD，只影响（反）三角函数
 * 复数开关：allow_complex == 0 时，出现虚部或需要复数才有意义的运算返回 CALC_DOMAIN
 */

#define MAX_DEPTH 16u /* 括号/函数嵌套上限，防止栈被递归吃光 */
#define WORD_MAX 6u
#define EPS 1e-6f

typedef struct
{
  const char *p;
  calc_angle_unit_t angle;
  uint8_t allow_complex;
  calc_status_t error;
  uint8_t depth;
} parser_t;

/* ---------------- 复数基础运算 ---------------- */

static calc_complex_t cplx(float re, float im)
{
  calc_complex_t v;
  v.real = re;
  v.imag = im;
  return v;
}

static calc_complex_t c_add(calc_complex_t a, calc_complex_t b)
{
  return cplx(a.real + b.real, a.imag + b.imag);
}

static calc_complex_t c_sub(calc_complex_t a, calc_complex_t b)
{
  return cplx(a.real - b.real, a.imag - b.imag);
}

static calc_complex_t c_mul(calc_complex_t a, calc_complex_t b)
{
  return cplx((a.real * b.real) - (a.imag * b.imag), (a.real * b.imag) + (a.imag * b.real));
}

static calc_complex_t c_neg(calc_complex_t a)
{
  return cplx(-a.real, -a.imag);
}

static float c_absf(calc_complex_t a)
{
  return hypotf(a.real, a.imag);
}

static uint8_t c_is_real(calc_complex_t a)
{
  return (fabsf(a.imag) < EPS) ? 1u : 0u;
}

static uint8_t c_is_zero(calc_complex_t a)
{
  return ((fabsf(a.real) < EPS) && (fabsf(a.imag) < EPS)) ? 1u : 0u;
}

static void set_error(parser_t *ps, calc_status_t code)
{
  if (ps->error == CALC_OK) /* 只保留第一个错误 */
  {
    ps->error = code;
  }
}

static void check_finite(parser_t *ps, calc_complex_t v)
{
  if ((!isfinite(v.real)) || (!isfinite(v.imag)))
  {
    set_error(ps, CALC_DOMAIN);
  }
}

/* 除法：分母为 0 单独报 CALC_DIV_ZERO */
static calc_complex_t c_div(parser_t *ps, calc_complex_t a, calc_complex_t b)
{
  float den;

  if (c_is_zero(b))
  {
    set_error(ps, CALC_DIV_ZERO);
    return cplx(0.0f, 0.0f);
  }
  den = (b.real * b.real) + (b.imag * b.imag);
  return cplx(((a.real * b.real) + (a.imag * b.imag)) / den,
              ((a.imag * b.real) - (a.real * b.imag)) / den);
}

/* exp(a)：e^(x+iy) = e^x · (cos y + i sin y) */
static calc_complex_t c_exp(parser_t *ps, calc_complex_t a)
{
  float m = expf(a.real);
  calc_complex_t r = cplx(m * cosf(a.imag), m * sinf(a.imag));
  check_finite(ps, r);
  return r;
}

/* ln(a) 主值：|a| 为 0 时定义域错误 */
static calc_complex_t c_log(parser_t *ps, calc_complex_t a)
{
  float m = c_absf(a);

  if (m < EPS)
  {
    set_error(ps, CALC_DOMAIN);
    return cplx(0.0f, 0.0f);
  }
  /*
   * 注意 -0.0：c_neg() 会把 +0.0 变成 -0.0，而 atan2f(-0.0, -1) = -π，
   * 会让 ln(-1) 变成 -πi。这里把 ±0 统一成 +0，保证 ln(-1) = +πi（主值）。
   */
  return cplx(logf(m), atan2f((a.imag == 0.0f) ? 0.0f : a.imag, a.real));
}

/* 复数的平方根主值 */
static calc_complex_t c_sqrt(parser_t *ps, calc_complex_t a)
{
  if ((c_is_real(a) != 0u) && (a.real >= 0.0f))
  {
    return cplx(sqrtf(a.real), 0.0f);
  }
  return c_exp(ps, c_mul(cplx(0.5f, 0.0f), c_log(ps, a)));
}

/* asin(z) = -i·ln(iz + sqrt(1-z²)) */
static calc_complex_t c_asin(parser_t *ps, calc_complex_t z)
{
  calc_complex_t iz = cplx(-z.imag, z.real);
  calc_complex_t root = c_sqrt(ps, c_sub(cplx(1.0f, 0.0f), c_mul(z, z)));
  calc_complex_t l = c_log(ps, c_add(iz, root));
  return cplx(l.imag, -l.real);
}

/* acos(z) = π/2 - asin(z) */
static calc_complex_t c_acos(parser_t *ps, calc_complex_t z)
{
  calc_complex_t a = c_asin(ps, z);
  return c_sub(cplx(1.57079633f, 0.0f), a);
}

/* atan(z) = (i/2)·(ln(1-iz) - ln(1+iz)) */
static calc_complex_t c_atan(parser_t *ps, calc_complex_t z)
{
  calc_complex_t iz = cplx(-z.imag, z.real);
  calc_complex_t a = c_log(ps, c_sub(cplx(1.0f, 0.0f), iz));
  calc_complex_t b = c_log(ps, c_add(cplx(1.0f, 0.0f), iz));
  calc_complex_t d = c_sub(a, b);
  return cplx(-0.5f * d.imag, 0.5f * d.real);
}

/* 复数幂：a^b = exp(b·ln a)，实数且指数是整数时直接用 powf */
static calc_complex_t c_pow(parser_t *ps, calc_complex_t a, calc_complex_t b)
{
  if (c_is_zero(a))
  {
    if (c_is_zero(b))
    {
      set_error(ps, CALC_DOMAIN);
      return cplx(0.0f, 0.0f);
    }
    if (b.real > 0.0f)
    {
      return cplx(0.0f, 0.0f);
    }
    set_error(ps, CALC_DIV_ZERO);
    return cplx(0.0f, 0.0f);
  }

  if ((c_is_real(a) != 0u) && (c_is_real(b) != 0u))
  {
    float base = a.real;
    float expo = b.real;

    if ((base > 0.0f) || (expo == floorf(expo)))
    {
      float r = powf(base, expo); /* 负底数 + 整数指数，powf 本身就有定义 */
      check_finite(ps, cplx(r, 0.0f));
      return cplx(r, 0.0f);
    }
  }
  return c_exp(ps, c_mul(b, c_log(ps, a)));
}

/* ---------------- 词法小工具 ---------------- */

static void skip_spaces(parser_t *ps)
{
  while ((*ps->p == ' ') || (*ps->p == '\t'))
  {
    ps->p++;
  }
}

static uint8_t accept_char(parser_t *ps, char ch)
{
  skip_spaces(ps);
  if (*ps->p == ch)
  {
    ps->p++;
    return 1u;
  }
  return 0u;
}

/* 匹配一个大小写不敏感的关键字（如 "sin"），成功则指针后移 */
static uint8_t match_word(parser_t *ps, const char *word)
{
  const char *save = ps->p;
  const char *w = word;

  skip_spaces(ps);
  save = ps->p;
  while (*w != '\0')
  {
    char c = *ps->p;
    char lower = ((c >= 'A') && (c <= 'Z')) ? (char)(c + 32) : c;

    if (lower != *w)
    {
      ps->p = save;
      return 0u;
    }
    ps->p++;
    w++;
  }
  return 1u;
}

/* 关键字后面不能紧跟字母数字，避免把 "e" 从 "exp" 里拆出来 */
static uint8_t word_boundary(parser_t *ps)
{
  char c = *ps->p;
  if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')))
  {
    return 0u;
  }
  return 1u;
}

/* 当前位置是否是一个"新的因子"的开头（用于隐式乘法判断） */
static uint8_t starts_primary(const char *p)
{
  char c = *p;

  if (((c >= '0') && (c <= '9')) || (c == '.') || (c == '('))
  {
    return 1u;
  }
  if (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')))
  {
    return 1u; /* 函数名/常数，用于隐式乘法：2sin(30) */
  }
  if (((unsigned char)c == 0xCFu) && ((unsigned char)p[1] == 0x80u))
  {
    return 1u; /* π */
  }
  if (((unsigned char)c == 0xE2u) && ((unsigned char)p[1] == 0x88u) &&
      ((unsigned char)p[2] == 0x9Au))
  {
    return 1u; /* √ */
  }
  /* 注意：极坐标符号 ∠(E2 88 A0) 是运算符，不能当因子开头，否则 5∠90 会被当成隐式乘法 */
  return 0u;
}

/* ---------------- 递归下降解析 ---------------- */

static calc_complex_t parse_expression(parser_t *ps);
static calc_complex_t parse_term(parser_t *ps);
static calc_complex_t parse_unary(parser_t *ps);
static calc_complex_t parse_power(parser_t *ps);
static calc_complex_t parse_primary(parser_t *ps);

/* 数字：123 / 1.5 / .5 / 2.5E-3 */
static calc_complex_t parse_number(parser_t *ps)
{
  float value = 0.0f;
  float scale = 0.1f;
  uint8_t digits = 0u;

  while ((*ps->p >= '0') && (*ps->p <= '9'))
  {
    value = (value * 10.0f) + (float)(*ps->p - '0');
    ps->p++;
    digits++;
  }
  if (*ps->p == '.')
  {
    ps->p++;
    while ((*ps->p >= '0') && (*ps->p <= '9'))
    {
      value += (float)(*ps->p - '0') * scale;
      scale *= 0.1f;
      ps->p++;
      digits++;
    }
  }
  if (digits == 0u)
  {
    set_error(ps, CALC_SYNTAX);
    return cplx(0.0f, 0.0f);
  }

  /* 科学计数法：E/e 后面必须真的有数字才算数 */
  if ((*ps->p == 'E') || (*ps->p == 'e'))
  {
    const char *save = ps->p;
    int8_t sign = 1;
    int exp = 0;

    ps->p++;
    if (*ps->p == '+')
    {
      ps->p++;
    }
    else if (*ps->p == '-')
    {
      sign = -1;
      ps->p++;
    }
    if ((*ps->p >= '0') && (*ps->p <= '9'))
    {
      while ((*ps->p >= '0') && (*ps->p <= '9'))
      {
        exp = (exp * 10) + (*ps->p - '0');
        if (exp > 38)
        {
          exp = 38;
        }
        ps->p++;
      }
      value *= powf(10.0f, (float)(sign * (int8_t)exp));
    }
    else
    {
      ps->p = save; /* 不是科学计数法，退回去 */
    }
  }

  check_finite(ps, cplx(value, 0.0f));
  return cplx(value, 0.0f);
}

/* 三角函数/对数等一元函数 */
typedef enum
{
  F_SIN = 0,
  F_COS,
  F_TAN,
  F_ASIN,
  F_ACOS,
  F_ATAN,
  F_SINH,
  F_COSH,
  F_TANH,
  F_LN,
  F_LOG,
  F_EXP,
  F_SQRT,
  F_ABS,
  F_ARG,
  F_CONJ,
  F_RE,
  F_IM
} func_id_t;

static const struct
{
  const char *name;
  uint8_t id;
} g_functions[] = {{"sin", F_SIN},   {"cos", F_COS},   {"tan", F_TAN},   {"asin", F_ASIN},
                   {"acos", F_ACOS}, {"atan", F_ATAN}, {"sinh", F_SINH}, {"cosh", F_COSH},
                   {"tanh", F_TANH}, {"ln", F_LN},     {"log", F_LOG},   {"lg", F_LOG},
                   {"exp", F_EXP},   {"sqrt", F_SQRT}, {"abs", F_ABS},   {"arg", F_ARG},
                   {"conj", F_CONJ}, {"re", F_RE},     {"im", F_IM}};

static float deg2rad(float d)
{
  return d * 0.0174532925f;
}

static float rad2deg(float r)
{
  return r * 57.29577951f;
}

static calc_complex_t apply_function(parser_t *ps, uint8_t id, calc_complex_t x)
{
  uint8_t deg = (ps->angle == CALC_ANGLE_DEG) ? 1u : 0u;
  calc_complex_t r = cplx(0.0f, 0.0f);

  /* 实参 + 实数模式：走 libm，最省事也最准 */
  if (c_is_real(x))
  {
    float v = x.real;

    switch (id)
    {
    case F_SIN:
      r = cplx(sinf(deg ? deg2rad(v) : v), 0.0f);
      break;
    case F_COS:
      r = cplx(cosf(deg ? deg2rad(v) : v), 0.0f);
      break;
    case F_TAN:
      r = cplx(tanf(deg ? deg2rad(v) : v), 0.0f);
      if (!isfinite(r.real))
      {
        set_error(ps, CALC_DOMAIN);
        r = cplx(0.0f, 0.0f);
      }
      break;
    case F_ASIN:
      if ((v < -1.0f) || (v > 1.0f))
      {
        if (ps->allow_complex == 0u)
        {
          set_error(ps, CALC_DOMAIN);
        }
        else
        {
          r = c_asin(ps, x); /* 复数支路：asin(2) = π/2 - i·acosh(2) */
        }
      }
      else
      {
        float a = asinf(v);
        r = cplx(deg ? rad2deg(a) : a, 0.0f);
      }
      break;
    case F_ACOS:
      if ((v < -1.0f) || (v > 1.0f))
      {
        if (ps->allow_complex == 0u)
        {
          set_error(ps, CALC_DOMAIN);
        }
        else
        {
          r = c_acos(ps, x);
        }
      }
      else
      {
        float a = acosf(v);
        r = cplx(deg ? rad2deg(a) : a, 0.0f);
      }
      break;
    case F_ATAN:
    {
      float a = atanf(v);
      r = cplx(deg ? rad2deg(a) : a, 0.0f);
      break;
    }
    case F_SINH:
      r = cplx(sinhf(v), 0.0f);
      break;
    case F_COSH:
      r = cplx(coshf(v), 0.0f);
      break;
    case F_TANH:
      r = cplx(tanhf(v), 0.0f);
      break;
    case F_LN:
      if (v <= 0.0f)
      {
        if (ps->allow_complex == 0u)
        {
          set_error(ps, CALC_DOMAIN);
        }
        else
        {
          r = c_log(ps, x);
        }
      }
      else
      {
        r = cplx(logf(v), 0.0f);
      }
      break;
    case F_LOG:
      if (v <= 0.0f)
      {
        if (ps->allow_complex == 0u)
        {
          set_error(ps, CALC_DOMAIN);
        }
        else
        {
          r = c_mul(c_log(ps, x), cplx(0.4342944819f, 0.0f));
        }
      }
      else
      {
        r = cplx(log10f(v), 0.0f);
      }
      break;
    case F_EXP:
      r = cplx(expf(v), 0.0f);
      break;
    case F_SQRT:
      if (v < 0.0f)
      {
        if (ps->allow_complex == 0u)
        {
          set_error(ps, CALC_DOMAIN);
        }
        else
        {
          r = cplx(0.0f, sqrtf(-v));
        }
      }
      else
      {
        r = cplx(sqrtf(v), 0.0f);
      }
      break;
    case F_ABS:
      r = cplx(fabsf(v), 0.0f);
      break;
    case F_ARG:
      r = cplx((v < 0.0f) ? (deg ? 180.0f : 3.14159265f) : 0.0f, 0.0f);
      break;
    case F_CONJ:
      r = x;
      break;
    case F_RE:
      r = x;
      break;
    case F_IM:
      r = cplx(0.0f, 0.0f);
      break;
    default:
      set_error(ps, CALC_SYNTAX);
      break;
    }
    check_finite(ps, r);
    return r;
  }

  /* 复数：下面这些按复变函数定义实现（角度统一用弧度） */
  switch (id)
  {
  case F_EXP:
    r = c_exp(ps, x);
    break;
  case F_LN:
    r = c_log(ps, x);
    break;
  case F_LOG:
    r = c_mul(c_log(ps, x), cplx(0.4342944819f, 0.0f));
    break;
  case F_SQRT:
    r = c_exp(ps, c_mul(cplx(0.5f, 0.0f), c_log(ps, x)));
    break;
  case F_ABS:
    r = cplx(c_absf(x), 0.0f);
    break;
  case F_ARG:
  {
    float a = atan2f(x.imag, x.real);
    r = cplx(deg ? rad2deg(a) : a, 0.0f);
    break;
  }
  case F_CONJ:
    r = cplx(x.real, -x.imag);
    break;
  case F_RE:
    r = cplx(x.real, 0.0f);
    break;
  case F_IM:
    r = cplx(x.imag, 0.0f);
    break;
  case F_SIN: /* sin(a+bi) = sin a·cosh b + i·cos a·sinh b */
    r = cplx(sinf(x.real) * coshf(x.imag), cosf(x.real) * sinhf(x.imag));
    break;
  case F_COS: /* cos(a+bi) = cos a·cosh b - i·sin a·sinh b */
    r = cplx(cosf(x.real) * coshf(x.imag), -sinf(x.real) * sinhf(x.imag));
    break;
  case F_TAN:
    r = c_div(ps, cplx(sinf(x.real) * coshf(x.imag), cosf(x.real) * sinhf(x.imag)),
              cplx(cosf(x.real) * coshf(x.imag), -sinf(x.real) * sinhf(x.imag)));
    break;
  case F_SINH:
    r = cplx(sinhf(x.real) * cosf(x.imag), coshf(x.real) * sinf(x.imag));
    break;
  case F_COSH:
    r = cplx(coshf(x.real) * cosf(x.imag), sinhf(x.real) * sinf(x.imag));
    break;
  case F_TANH:
    r = c_div(ps, cplx(sinhf(x.real) * cosf(x.imag), coshf(x.real) * sinf(x.imag)),
              cplx(coshf(x.real) * cosf(x.imag), sinhf(x.real) * sinf(x.imag)));
    break;
  case F_ASIN:
    r = c_asin(ps, x);
    break;
  case F_ACOS:
    r = c_acos(ps, x);
    break;
  case F_ATAN:
    r = c_atan(ps, x);
    break;
  default:
    set_error(ps, CALC_DOMAIN); /* 其余函数的复变形式本实现不支持 */
    r = cplx(0.0f, 0.0f);
    break;
  }
  check_finite(ps, r);
  return r;
}

static calc_complex_t parse_primary(parser_t *ps)
{
  calc_complex_t value = cplx(0.0f, 0.0f);
  uint32_t i;

  skip_spaces(ps);

  /* 括号 */
  if (*ps->p == '(')
  {
    ps->p++;
    if (ps->depth >= MAX_DEPTH)
    {
      set_error(ps, CALC_SYNTAX);
      return value;
    }
    ps->depth++;
    value = parse_expression(ps);
    ps->depth--;
    if (accept_char(ps, ')') == 0u)
    {
      set_error(ps, CALC_SYNTAX);
    }
    return value;
  }

  /* 数字 */
  if (((*ps->p >= '0') && (*ps->p <= '9')) || (*ps->p == '.'))
  {
    return parse_number(ps);
  }

  /* UTF-8：π 是 CF 80；√ 是 E2 88 9A（∠ 是 E2 88 A0，在表达式层处理） */
  if ((unsigned char)ps->p[0] == 0xCFu)
  {
    if ((unsigned char)ps->p[1] == 0x80u)
    {
      ps->p += 2;
      return cplx(3.14159265f, 0.0f); /* π */
    }
    set_error(ps, CALC_SYNTAX);
    return value;
  }
  if ((unsigned char)ps->p[0] == 0xE2u)
  {
    if (((unsigned char)ps->p[1] == 0x88u) && ((unsigned char)ps->p[2] == 0x9Au))
    {
      ps->p += 3; /* √ 后面直接跟一个因子：√9、√(2+7) */
      value = parse_unary(ps);
      return apply_function(ps, F_SQRT, value);
    }
    set_error(ps, CALC_SYNTAX);
    return value;
  }

  /* 函数名 */
  for (i = 0u; i < (sizeof(g_functions) / sizeof(g_functions[0])); i++)
  {
    const char *save = ps->p;

    if (match_word(ps, g_functions[i].name) != 0u)
    {
      if (accept_char(ps, '(') != 0u)
      {
        calc_complex_t arg;

        if (ps->depth >= MAX_DEPTH)
        {
          set_error(ps, CALC_SYNTAX);
          return value;
        }
        ps->depth++;
        arg = parse_expression(ps);
        ps->depth--;
        if (accept_char(ps, ')') == 0u)
        {
          set_error(ps, CALC_SYNTAX);
        }
        return apply_function(ps, g_functions[i].id, arg);
      }
      ps->p = save; /* 只是名字的一部分，交给常数分支 */
    }
  }

  /* 常数：pi / π / e / i */
  if (match_word(ps, "pi") != 0u)
  {
    if (word_boundary(ps) != 0u)
    {
      return cplx(3.14159265f, 0.0f);
    }
  }
  if (match_word(ps, "e") != 0u)
  {
    if (word_boundary(ps) != 0u)
    {
      return cplx(2.71828183f, 0.0f);
    }
  }
  if (match_word(ps, "i") != 0u)
  {
    if (word_boundary(ps) != 0u)
    {
      if (ps->allow_complex == 0u)
      {
        set_error(ps, CALC_DOMAIN);
      }
      return cplx(0.0f, 1.0f);
    }
  }

  set_error(ps, CALC_SYNTAX);
  return cplx(0.0f, 0.0f);
}

static calc_complex_t parse_unary(parser_t *ps)
{
  skip_spaces(ps);
  if (accept_char(ps, '+') != 0u)
  {
    return parse_unary(ps);
  }
  if (accept_char(ps, '-') != 0u)
  {
    return c_neg(parse_unary(ps)); /* -2^2 = -(2^2)，负号优先级低于乘方 */
  }
  return parse_power(ps);
}

static calc_complex_t parse_power(parser_t *ps)
{
  calc_complex_t base = parse_primary(ps);

  skip_spaces(ps);
  if (*ps->p == '^')
  {
    calc_complex_t exponent;

    ps->p++;
    if (ps->depth >= MAX_DEPTH)
    {
      set_error(ps, CALC_SYNTAX);
      return base;
    }
    ps->depth++;
    exponent = parse_unary(ps); /* 右结合：2^3^2 = 2^(3^2)；且允许 2^-3 */
    ps->depth--;
    base = c_pow(ps, base, exponent);
  }
  return base;
}

static calc_complex_t parse_term(parser_t *ps)
{
  calc_complex_t value = parse_unary(ps);

  for (;;)
  {
    skip_spaces(ps);
    if (ps->error != CALC_OK)
    {
      break;
    }
    if (*ps->p == '*')
    {
      ps->p++;
      value = c_mul(value, parse_unary(ps));
    }
    else if ((*ps->p == '/') || (*ps->p == ':'))
    {
      calc_complex_t rhs;
      ps->p++;
      rhs = parse_unary(ps);
      value = c_div(ps, value, rhs);
    }
    else if (starts_primary(ps->p) != 0u)
    {
      /* 隐式乘法：2(3+1)、3sin(30)、2i */
      value = c_mul(value, parse_unary(ps));
    }
    else
    {
      break;
    }
    check_finite(ps, value);
  }
  return value;
}

static calc_complex_t parse_expression(parser_t *ps)
{
  calc_complex_t value = parse_term(ps);

  for (;;)
  {
    skip_spaces(ps);
    if (ps->error != CALC_OK)
    {
      break;
    }
    if (*ps->p == '+')
    {
      ps->p++;
      value = c_add(value, parse_term(ps));
    }
    else if (*ps->p == '-')
    {
      ps->p++;
      value = c_sub(value, parse_term(ps));
    }
    else if (((unsigned char)ps->p[0] == 0xE2u) && ((unsigned char)ps->p[1] == 0x88u) &&
             ((unsigned char)ps->p[2] == 0xA0u))
    {
      /* 极坐标：a∠b = a·(cos b + i·sin b) */
      calc_complex_t angle;
      float rad;

      ps->p += 3;
      angle = parse_term(ps);
      if (c_is_real(angle) == 0u)
      {
        set_error(ps, CALC_DOMAIN);
        break;
      }
      rad = (ps->angle == CALC_ANGLE_DEG) ? deg2rad(angle.real) : angle.real;
      value = c_mul(value, cplx(cosf(rad), sinf(rad)));
    }
    else
    {
      break;
    }
    check_finite(ps, value);
  }
  return value;
}

/* ---------------- 对外接口 ---------------- */

calc_status_t calculator_evaluate(const char *expression, calc_angle_unit_t angle_unit,
                                  uint8_t allow_complex, calc_complex_t answer,
                                  calc_complex_t *result)
{
  parser_t ps;
  calc_complex_t value;

  (void)answer; /* 官方头文件里这个参数是按值传入的，本实现的返回值走 result */

  if ((expression == 0) || (result == 0))
  {
    return CALC_SYNTAX;
  }

  ps.p = expression;
  ps.angle = angle_unit;
  ps.allow_complex = allow_complex;
  ps.error = CALC_OK;
  ps.depth = 0u;

  value = parse_expression(&ps);
  skip_spaces(&ps);

  if (ps.error != CALC_OK)
  {
    return ps.error;
  }
  if (*ps.p != '\0')
  {
    return CALC_SYNTAX; /* 还有没吃掉的字符，说明表达式不合法 */
  }
  if ((allow_complex == 0u) && (c_is_real(value) == 0u))
  {
    return CALC_DOMAIN;
  }
  if ((!isfinite(value.real)) || (!isfinite(value.imag)))
  {
    return CALC_DOMAIN;
  }

  *result = value;
  return CALC_OK;
}

calc_status_t calculator_solve_linear(float a, float b, float *x)
{
  if (x == 0)
  {
    return CALC_SYNTAX;
  }
  if (fabsf(a) < EPS)
  {
    return (fabsf(b) < EPS) ? CALC_DOMAIN : CALC_DIV_ZERO;
  }
  *x = -b / a;
  return CALC_OK;
}

/* ---------------- 结果格式化（不依赖 printf，也不用 %f） ---------------- */

static uint32_t pow10u(uint8_t n)
{
  uint32_t v = 1u;
  while (n-- > 0u)
  {
    v *= 10u;
  }
  return v;
}

/* 定点写法：整数部分 + 最多 decimals 位小数（末尾 0 去掉），返回长度 */
static uint8_t fixed_try(float a, uint8_t decimals, char *out)
{
  uint8_t len = 0u;
  uint8_t i;
  uint8_t n = 0u;
  uint8_t kept = 0u;
  uint32_t ip = (uint32_t)a;
  float frac = a - (float)ip;
  uint32_t frac_scaled = 0u;
  uint32_t limit;
  char digits[8];
  char stack[12];

  if (decimals > 0u)
  {
    limit = pow10u(decimals);
    frac_scaled = (uint32_t)((frac * (float)limit) + 0.5f);
    if (frac_scaled >= limit) /* 小数进位到整数 */
    {
      frac_scaled -= limit;
      ip++;
    }
  }
  else if (frac >= 0.5f)
  {
    ip++;
  }

  /* 整数部分（倒序取位） */
  do
  {
    stack[n++] = (char)('0' + (ip % 10u));
    ip /= 10u;
  } while ((ip != 0u) && (n < 12u));
  while (n > 0u)
  {
    out[len++] = stack[--n];
  }

  /* 小数部分 */
  if (decimals > 0u)
  {
    for (i = 0u; i < decimals; i++)
    {
      digits[i] = (char)('0' + ((frac_scaled / pow10u((uint8_t)(decimals - 1u - i))) % 10u));
    }
    while ((kept < decimals) && (digits[decimals - 1u - kept] == '0'))
    {
      kept++;
    }
    if (kept < decimals)
    {
      out[len++] = '.';
      for (i = 0u; i < (uint8_t)(decimals - kept); i++)
      {
        out[len++] = digits[i];
      }
    }
  }
  return len;
}

static uint8_t fmt_fixed(float a, uint8_t budget, char *out)
{
  uint8_t int_digits = 1u;
  uint8_t decimals;
  uint8_t len;
  uint32_t probe = (uint32_t)a;

  while (probe >= 10u)
  {
    probe /= 10u;
    int_digits++;
  }
  decimals = (budget > (uint8_t)(int_digits + 1u)) ? (uint8_t)(budget - int_digits - 1u) : 0u;
  if (decimals > 6u)
  {
    decimals = 6u;
  }

  for (;;)
  {
    len = fixed_try(a, decimals, out);
    if ((len <= budget) || (decimals == 0u))
    {
      return len;
    }
    decimals--;
  }
}

/* 科学计数法写法：d.dddE±xx，返回长度 */
static uint8_t sci_try(float a, uint8_t decimals, char *out)
{
  uint8_t len = 0u;
  uint8_t i;
  uint8_t e_digits = 1u;
  uint16_t abs_exp;
  int16_t exp = 0;
  float mant = a;
  uint32_t limit;
  uint32_t scaled;
  uint32_t ip;
  uint32_t frac_scaled = 0u;
  uint8_t kept = 0u;
  char digits[8];

  while (mant >= 10.0f)
  {
    mant /= 10.0f;
    exp++;
  }
  while (mant < 1.0f)
  {
    mant *= 10.0f;
    exp--;
  }

  limit = pow10u(decimals);
  scaled = (uint32_t)((mant * (float)limit) + 0.5f);
  if (scaled >= (10u * limit)) /* 9.9999 -> 10.0：归一化到 1.0 并让指数 +1 */
  {
    scaled = limit;
    exp++;
  }
  ip = scaled / limit;
  frac_scaled = scaled % limit;

  for (i = 0u; i < decimals; i++)
  {
    digits[i] = (char)('0' + ((frac_scaled / pow10u((uint8_t)(decimals - 1u - i))) % 10u));
  }
  while ((kept < decimals) && (digits[decimals - 1u - kept] == '0'))
  {
    kept++;
  }

  out[len++] = (char)('0' + (ip % 10u));
  if (kept < decimals)
  {
    out[len++] = '.';
    for (i = 0u; i < (uint8_t)(decimals - kept); i++)
    {
      out[len++] = digits[i];
    }
  }
  out[len++] = 'E';
  if (exp < 0)
  {
    out[len++] = '-';
    abs_exp = (uint16_t)(-exp);
  }
  else
  {
    abs_exp = (uint16_t)exp;
  }
  if (abs_exp >= 100u)
  {
    e_digits = 3u;
  }
  else if (abs_exp >= 10u)
  {
    e_digits = 2u;
  }
  if (e_digits == 3u)
  {
    out[len++] = (char)('0' + ((abs_exp / 100u) % 10u));
  }
  if (e_digits >= 2u)
  {
    out[len++] = (char)('0' + ((abs_exp / 10u) % 10u));
  }
  out[len++] = (char)('0' + (abs_exp % 10u));
  return len;
}

static uint8_t fmt_scientific(float a, uint8_t budget, char *out)
{
  uint8_t d;
  uint8_t len;

  for (d = 6u; d > 0u; d--)
  {
    len = sci_try(a, d, out);
    if (len <= budget)
    {
      return len;
    }
  }
  return sci_try(a, 0u, out);
}

/* 非负浮点数 -> 字符串：数量级合适用定点，太大/太小用科学计数法 */
static uint8_t fmt_positive(float a, uint8_t budget, char *out)
{
  if ((a != 0.0f) && ((a >= 1.0e7f) || (a < 1.0e-3f)))
  {
    return fmt_scientific(a, budget, out);
  }
  return fmt_fixed(a, budget, out);
}

static uint8_t fmt_real(float v, uint8_t budget, char *out)
{
  uint8_t len = 0u;

  if (budget > 20u)
  {
    budget = 20u;
  }
  if (isnan(v))
  {
    out[0] = 'N';
    out[1] = 'a';
    out[2] = 'N';
    return 3u;
  }
  if (isinf(v))
  {
    out[0] = 'I';
    out[1] = 'n';
    out[2] = 'f';
    return 3u;
  }
  if (v < 0.0f)
  {
    out[len++] = '-';
    if (budget > 0u)
    {
      budget--;
    }
  }
  if (budget == 0u)
  {
    return len;
  }
  len = (uint8_t)(len + fmt_positive(fabsf(v), budget, &out[len]));
  return len;
}

uint8_t calculator_format(const calc_complex_t *value, uint8_t max_chars, char *out)
{
  uint8_t len;

  if ((value == 0) || (out == 0) || (max_chars == 0u))
  {
    return 0u;
  }
  if (max_chars > 24u)
  {
    max_chars = 24u;
  }

  /* 纯实数：直接用满整行 */
  if (c_is_real(*value) != 0u)
  {
    return fmt_real(value->real, max_chars, out);
  }

  /* 纯虚数：2i */
  if (fabsf(value->real) < EPS)
  {
    len = fmt_real(value->imag, (uint8_t)(max_chars - 1u), out);
    out[len++] = 'i';
    return len;
  }

  /* a+bi：先排版虚部，剩下的宽度留给实部 */
  {
    char imag_buf[12];
    uint8_t imag_len;
    uint8_t imag_budget = (uint8_t)((max_chars > 6u) ? ((max_chars - 2u) / 2u) : 2u);
    uint8_t i;

    if (imag_budget > 10u)
    {
      imag_budget = 10u;
    }
    imag_len = fmt_real(fabsf(value->imag), imag_budget, imag_buf);
    len = fmt_real(value->real, (uint8_t)(max_chars - imag_len - 2u), out);
    out[len++] = (value->imag < 0.0f) ? '-' : '+';
    for (i = 0u; i < imag_len; i++)
    {
      out[len++] = imag_buf[i];
    }
    out[len++] = 'i';
  }
  return len;
}
