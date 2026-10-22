#ifndef CRY_FACE_H
#define CRY_FACE_H

#include <stdint.h>

/*
 * 创新点二：用 CGRAM 哭脸字符替代 "ERROR" 文本。
 *
 * - 屏幕字符槽位 2（与 lcd1602 的 LCD1602_CHAR_ANGLE=1 相邻）；
 * - cry_face_init() 需要在 lcd1602_init() 之后调用一次；
 * - cry_face_replace_error() 在写屏前处理两行缓冲。
 */

#define CRY_FACE_SLOT 2u

void    cry_face_init(void);
uint8_t cry_face_replace_error(char line[16]);

#endif /* CRY_FACE_H */
