#include "cry_face.h"
#include "lcd1602.h"
#include <string.h>

/* 5x8 点阵：两只眼睛 + 往下弯的嘴 */
static const uint8_t k_cry_char[8] =
{
  0x00u, 0x0Au, 0x0Au, 0x11u, 0x00u, 0x0Eu, 0x11u, 0x00u
};

void cry_face_init(void)
{
  lcd1602_define_char(CRY_FACE_SLOT, k_cry_char);
}

uint8_t cry_face_replace_error(char line[16])
{
  uint8_t i;

  for (i = 0u; i <= 11u; i++)
  {
    if ((line[i] == 'E') && (line[i + 1u] == 'R') && (line[i + 2u] == 'R') &&
        (line[i + 3u] == 'O') && (line[i + 4u] == 'R'))
    {
      memset(line, ' ', 16u);
      line[i] = (char)CRY_FACE_SLOT;
      return 1u;
    }
  }
  return 0u;
}
