#include "calculator_engine.h"
#include "touch_filter.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define PI_F 3.14159265f
#define TOLERANCE 2.0e-3f

static int failures;

#define CHECK(condition)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(condition))                                                                              \
    {                                                                                              \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition);                \
      failures++;                                                                                  \
    }                                                                                              \
  } while (0)

static int nearly_equal(float actual, float expected)
{
  float delta = fabsf(actual - expected);
  float scale = fmaxf(1.0f, fmaxf(fabsf(actual), fabsf(expected)));
  return delta <= TOLERANCE * scale;
}

static void check_expression(const char *expression, calc_angle_unit_t angle, uint8_t allow_complex,
                             calc_status_t expected_status, float expected_real,
                             float expected_imag)
{
  calc_complex_t answer = {0.0f, 0.0f};
  calc_complex_t result = {NAN, NAN};
  calc_status_t status = calculator_evaluate(expression, angle, allow_complex, answer, &result);

  if (status != expected_status)
  {
    fprintf(stderr, "expression '%s': status %d, expected %d\n", expression, status,
            expected_status);
    failures++;
    return;
  }

  if ((status == CALC_OK) &&
      ((!nearly_equal(result.real, expected_real)) || (!nearly_equal(result.imag, expected_imag))))
  {
    fprintf(stderr, "expression '%s': got (%.7g, %.7g), expected (%.7g, %.7g)\n", expression,
            result.real, result.imag, expected_real, expected_imag);
    failures++;
  }
}

static void test_calculator_engine(void)
{
  calc_complex_t answer = {0.0f, 0.0f};
  calc_complex_t result = {0.0f, 0.0f};
  float root = 0.0f;

  CHECK(calculator_evaluate(NULL, CALC_ANGLE_DEG, 1u, answer, &result) == CALC_SYNTAX);
  CHECK(calculator_evaluate("1", CALC_ANGLE_DEG, 1u, answer, NULL) == CALC_SYNTAX);

  check_expression("1 + 2*3", CALC_ANGLE_DEG, 1u, CALC_OK, 7.0f, 0.0f);
  check_expression("(1+2)*3", CALC_ANGLE_DEG, 1u, CALC_OK, 9.0f, 0.0f);
  check_expression("2^3^2", CALC_ANGLE_DEG, 1u, CALC_OK, 512.0f, 0.0f);
  check_expression("-2^2", CALC_ANGLE_DEG, 1u, CALC_OK, -4.0f, 0.0f);
  check_expression("+---5", CALC_ANGLE_DEG, 1u, CALC_OK, -5.0f, 0.0f);
  check_expression("2(3+1)", CALC_ANGLE_DEG, 1u, CALC_OK, 8.0f, 0.0f);
  check_expression("3 : 2", CALC_ANGLE_DEG, 1u, CALC_OK, 1.5f, 0.0f);
  check_expression(".5 + 1.5", CALC_ANGLE_DEG, 1u, CALC_OK, 2.0f, 0.0f);
  check_expression("1.25e3", CALC_ANGLE_DEG, 1u, CALC_OK, 1250.0f, 0.0f);
  check_expression("1e+2", CALC_ANGLE_DEG, 1u, CALC_OK, 100.0f, 0.0f);
  check_expression("2.5E-3", CALC_ANGLE_DEG, 1u, CALC_OK, 0.0025f, 0.0f);
  check_expression("2e", CALC_ANGLE_DEG, 1u, CALC_OK, 5.43656366f, 0.0f);
  check_expression("pi", CALC_ANGLE_DEG, 1u, CALC_OK, PI_F, 0.0f);
  check_expression("PI", CALC_ANGLE_DEG, 1u, CALC_OK, PI_F, 0.0f);
  check_expression("π", CALC_ANGLE_DEG, 1u, CALC_OK, PI_F, 0.0f);
  check_expression("√9", CALC_ANGLE_DEG, 1u, CALC_OK, 3.0f, 0.0f);

  check_expression("sin(30)", CALC_ANGLE_DEG, 1u, CALC_OK, 0.5f, 0.0f);
  check_expression("cos(60)", CALC_ANGLE_DEG, 1u, CALC_OK, 0.5f, 0.0f);
  check_expression("tan(45)", CALC_ANGLE_DEG, 1u, CALC_OK, 1.0f, 0.0f);
  check_expression("asin(1)", CALC_ANGLE_DEG, 1u, CALC_OK, 90.0f, 0.0f);
  check_expression("acos(0)", CALC_ANGLE_DEG, 1u, CALC_OK, 90.0f, 0.0f);
  check_expression("atan(1)", CALC_ANGLE_DEG, 1u, CALC_OK, 45.0f, 0.0f);
  check_expression("sin(pi/2)", CALC_ANGLE_RAD, 1u, CALC_OK, 1.0f, 0.0f);
  check_expression("cos(pi)", CALC_ANGLE_RAD, 1u, CALC_OK, -1.0f, 0.0f);
  check_expression("sinh(0)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, 0.0f);
  check_expression("cosh(0)", CALC_ANGLE_RAD, 1u, CALC_OK, 1.0f, 0.0f);
  check_expression("tanh(0)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, 0.0f);
  check_expression("ln(e)", CALC_ANGLE_RAD, 1u, CALC_OK, 1.0f, 0.0f);
  check_expression("log(100)", CALC_ANGLE_RAD, 1u, CALC_OK, 2.0f, 0.0f);
  check_expression("lg(1000)", CALC_ANGLE_RAD, 1u, CALC_OK, 3.0f, 0.0f);
  check_expression("exp(0)", CALC_ANGLE_RAD, 1u, CALC_OK, 1.0f, 0.0f);
  check_expression("sqrt(9)", CALC_ANGLE_RAD, 1u, CALC_OK, 3.0f, 0.0f);
  check_expression("abs(-3)", CALC_ANGLE_RAD, 1u, CALC_OK, 3.0f, 0.0f);
  check_expression("arg(-1)", CALC_ANGLE_RAD, 1u, CALC_OK, PI_F, 0.0f);
  check_expression("arg(-1)", CALC_ANGLE_DEG, 1u, CALC_OK, 180.0f, 0.0f);
  check_expression("conj(3+4i)", CALC_ANGLE_RAD, 1u, CALC_OK, 3.0f, -4.0f);
  check_expression("re(3+4i)", CALC_ANGLE_RAD, 1u, CALC_OK, 3.0f, 0.0f);
  check_expression("im(3+4i)", CALC_ANGLE_RAD, 1u, CALC_OK, 4.0f, 0.0f);
  check_expression("3sin(30)", CALC_ANGLE_DEG, 1u, CALC_OK, 1.5f, 0.0f);

  check_expression("i*i", CALC_ANGLE_RAD, 1u, CALC_OK, -1.0f, 0.0f);
  check_expression("sqrt(-4)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, 2.0f);
  check_expression("ln(-1)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, PI_F);
  check_expression("log(-1)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, 1.36437635f);
  check_expression("exp(i)", CALC_ANGLE_RAD, 1u, CALC_OK, cosf(1.0f), sinf(1.0f));
  check_expression("sin(i)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, sinhf(1.0f));
  check_expression("cos(i)", CALC_ANGLE_RAD, 1u, CALC_OK, coshf(1.0f), 0.0f);
  check_expression("tan(i)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, tanhf(1.0f));
  check_expression("sinh(i)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, sinf(1.0f));
  check_expression("cosh(i)", CALC_ANGLE_RAD, 1u, CALC_OK, cosf(1.0f), 0.0f);
  check_expression("tanh(i)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.0f, tanf(1.0f));
  check_expression("sqrt(i)", CALC_ANGLE_RAD, 1u, CALC_OK, 0.70710678f, 0.70710678f);
  check_expression("abs(3+4i)", CALC_ANGLE_RAD, 1u, CALC_OK, 5.0f, 0.0f);
  check_expression("arg(i)", CALC_ANGLE_RAD, 1u, CALC_OK, PI_F / 2.0f, 0.0f);
  check_expression("arg(i)", CALC_ANGLE_DEG, 1u, CALC_OK, 90.0f, 0.0f);
  check_expression("3∠90", CALC_ANGLE_DEG, 1u, CALC_OK, 0.0f, 3.0f);

  check_expression("1/0", CALC_ANGLE_RAD, 1u, CALC_DIV_ZERO, 0.0f, 0.0f);
  check_expression("0^0", CALC_ANGLE_RAD, 1u, CALC_DOMAIN, 0.0f, 0.0f);
  check_expression("sqrt(-1)", CALC_ANGLE_RAD, 0u, CALC_DOMAIN, 0.0f, 0.0f);
  check_expression("ln(-1)", CALC_ANGLE_RAD, 0u, CALC_DOMAIN, 0.0f, 0.0f);
  check_expression("i", CALC_ANGLE_RAD, 0u, CALC_DOMAIN, 0.0f, 0.0f);
  check_expression("1+", CALC_ANGLE_RAD, 1u, CALC_SYNTAX, 0.0f, 0.0f);
  check_expression("(1", CALC_ANGLE_RAD, 1u, CALC_SYNTAX, 0.0f, 0.0f);
  check_expression("()", CALC_ANGLE_RAD, 1u, CALC_SYNTAX, 0.0f, 0.0f);
  check_expression("unknown(1)", CALC_ANGLE_RAD, 1u, CALC_SYNTAX, 0.0f, 0.0f);
  check_expression("(((((((((((((((((((1)))))))))))))))))))", CALC_ANGLE_RAD, 1u, CALC_SYNTAX, 0.0f,
                   0.0f);
  check_expression("i∠90", CALC_ANGLE_DEG, 1u, CALC_OK, -1.0f, 0.0f);

  CHECK(calculator_solve_linear(2.0f, -6.0f, &root) == CALC_OK);
  CHECK(nearly_equal(root, 3.0f));
  CHECK(calculator_solve_linear(0.0f, 1.0f, &root) == CALC_DIV_ZERO);
  CHECK(calculator_solve_linear(0.0f, 0.0f, &root) == CALC_DOMAIN);
  CHECK(calculator_solve_linear(1.0f, 1.0f, NULL) == CALC_SYNTAX);
}

static void check_format(const calc_complex_t *value, uint8_t width, const char *expected)
{
  char output[32] = {0};
  uint8_t length = calculator_format(value, width, output);

  if ((length != strlen(expected)) || (strcmp(output, expected) != 0))
  {
    fprintf(stderr, "format: got '%s' (%u), expected '%s' (%zu)\n", output, length, expected,
            strlen(expected));
    failures++;
  }
}

static void test_format(void)
{
  calc_complex_t value;
  char output[8] = {0};

  CHECK(calculator_format(NULL, 16u, output) == 0u);
  CHECK(calculator_format(&value, 0u, output) == 0u);

  value = (calc_complex_t){42.0f, 0.0f};
  check_format(&value, 16u, "42");
  value = (calc_complex_t){-0.25f, 0.0f};
  check_format(&value, 16u, "-0.25");
  value = (calc_complex_t){12345678.0f, 0.0f};
  check_format(&value, 16u, "1.234568E7");
  value = (calc_complex_t){0.0001f, 0.0f};
  check_format(&value, 16u, "1E-4");
  value = (calc_complex_t){0.0f, 2.0f};
  check_format(&value, 16u, "2i");
  value = (calc_complex_t){3.0f, 4.0f};
  check_format(&value, 16u, "3+4i");
  value = (calc_complex_t){3.0f, -4.0f};
  check_format(&value, 16u, "3-4i");
  value = (calc_complex_t){NAN, 0.0f};
  check_format(&value, 16u, "NaN");
  value = (calc_complex_t){INFINITY, 0.0f};
  check_format(&value, 16u, "Inf");
}

static void test_touch_filter(void)
{
  touch_filter_t filter;

  touch_filter_init(NULL);
  CHECK(touch_filter_update(NULL, 1u) == 0u);

  touch_filter_init(&filter);
  CHECK(touch_filter_update(&filter, 0u) == 0u);
  CHECK(touch_filter_update(&filter, 1u) == 0u);
  CHECK(touch_filter_update(&filter, 1u) == 0u);
  CHECK(touch_filter_update(&filter, 1u) == 1u);
  CHECK(touch_filter_held(&filter) == 1u);
  CHECK(touch_filter_update(&filter, 1u) == 0u);
  CHECK(touch_filter_update(&filter, 0u) == 0u);
  CHECK(touch_filter_update(&filter, 0u) == 0u);
  CHECK(touch_filter_update(&filter, 0u) == 0u);
  CHECK(touch_filter_held(&filter) == 0u);

  CHECK(touch_filter_update(&filter, 5u) == 0u);
  CHECK(touch_filter_update(&filter, 5u) == 0u);
  CHECK(touch_filter_update(&filter, 5u) == 1u);
  CHECK(touch_filter_held(&filter) == 5u);
  CHECK(touch_filter_update(&filter, 4u) == 0u);
  CHECK(touch_filter_update(&filter, 0u) == 0u);
  CHECK(touch_filter_update(&filter, 0u) == 0u);
  CHECK(touch_filter_update(&filter, 0u) == 0u);

  CHECK(touch_filter_update(&filter, 1u << 29) == 0u);
  CHECK(touch_filter_update(&filter, 1u << 29) == 0u);
  CHECK(touch_filter_update(&filter, 1u << 29) == (1u << 29));
}

int main(void)
{
  test_calculator_engine();
  test_format();
  test_touch_filter();

  if (failures != 0)
  {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }

  printf("All core tests passed\n");
  return 0;
}
