#include "cry_face.h"
#include "lcd1602.h"

#include <stdio.h>
#include <string.h>

static int failures;
static uint8_t captured_slot;
static uint8_t captured_rows[8];

#define CHECK(condition)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(condition))                                                                              \
    {                                                                                              \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition);                \
      failures++;                                                                                  \
    }                                                                                              \
  } while (0)

void lcd1602_define_char(uint8_t slot, const uint8_t rows[8])
{
  memcpy(captured_rows, rows, sizeof(captured_rows));
  captured_slot = slot;
}

static void test_init(void)
{
  cry_face_init();

  CHECK(captured_slot == CRY_FACE_SLOT);
  CHECK(captured_rows[0] == 0x00u);
  CHECK(captured_rows[1] == 0x0Au);
  CHECK(captured_rows[2] == 0x0Au);
  CHECK(captured_rows[3] == 0x11u);
  CHECK(captured_rows[4] == 0x00u);
  CHECK(captured_rows[5] == 0x0Eu);
  CHECK(captured_rows[6] == 0x11u);
  CHECK(captured_rows[7] == 0x00u);
}

static void test_replace(void)
{
  char line[16] = "Syntax ERROR    ";

  CHECK(cry_face_replace_error(line) == 1u);
  CHECK(line[7] == (char)CRY_FACE_SLOT);
  CHECK(line[0] == ' ');
  CHECK(line[15] == ' ');
}

static void test_preserve_ready(void)
{
  char line[16] = "READY           ";
  char expected[16] = "READY           ";

  CHECK(cry_face_replace_error(line) == 0u);
  CHECK(memcmp(line, expected, sizeof(line)) == 0);
}

static void test_match_bounds(void)
{
  char start[16] = "ERROR           ";
  char end[16] = "           ERROR";

  CHECK(cry_face_replace_error(start) == 1u);
  CHECK(start[0] == (char)CRY_FACE_SLOT);

  CHECK(cry_face_replace_error(end) == 1u);
  CHECK(end[11] == (char)CRY_FACE_SLOT);
}

int main(void)
{
  test_init();
  test_replace();
  test_preserve_ready();
  test_match_bounds();

  if (failures != 0)
  {
    fprintf(stderr, "%d cry-face test(s) failed\n", failures);
    return 1;
  }

  printf("All cry-face tests passed\n");
  return 0;
}
