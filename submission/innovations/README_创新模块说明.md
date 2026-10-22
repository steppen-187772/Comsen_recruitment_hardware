# 创新模块说明：数字雨锁屏 + 哭脸报错

> 这两个模块来自 `build/innovation_lockrain_v4`（数字雨锁屏固件），
> 已从工程中拆成独立源码，与 `code/src` 下的其它模块并列放置。
> 它们**不参与**稳定计算器固件（保三换三）的构建，单独给创新版本使用。

---

## 1. 文件

| 文件 | 作用 |
| --- | --- |
| `src/lock_screen.c` / `include/lock_screen.h` | 数字雨锁屏（8 个数字粒子、上下两行、MODE 解锁） |
| `src/cry_face.c` / `include/cry_face.h` | 用 CGRAM 哭脸字符替代 `ERROR` 文本 |
| `tests/test_innovation.c` | 主机测试（18 项断言） |

## 2. 依赖

- `lcd1602_write_lines()`（刷新两行 16 字符）
- `lcd1602_define_char()`（写 CGRAM，槽位 2）
- `HAL_GetTick()`（随机种子与时间基准）

## 3. 集成步骤

### 3.1 数字雨锁屏

1. 初始化液晶后调用：

```c
lcd1602_init();
lock_screen_init();
```

2. 在主循环/10ms 任务里推进数字雨：

```c
lock_screen_tick(HAL_GetTick());
```

3. 按键事件先交给锁屏判断，返回 1 表示被锁屏消费：

```c
if (lock_screen_handle_key(edge, filter.stable_bitmap) != 0u)
{
    return;        /* 不要继续交给计算器 */
}
```

4. 锁屏期间不要让应用刷屏（否则数字雨会被覆盖）：

```c
void lcd1602_write_frame(...)
{
    if (lock_screen_is_locked() != 0u)
    {
        return;
    }
    ...
}
```

### 3.2 哭脸报错

1. 在 `lcd1602_init()` 里注册哭脸字符：

```c
cry_face_init();       /* CGRAM 槽位 2 */
```

2. 写屏前处理两行缓冲：

```c
char l1[16], l2[16];
memcpy(l1, line1, 16);
memcpy(l2, line2, 16);
cry_face_replace_error(l1);
cry_face_replace_error(l2);
lcd1602_write_lines(l1, l2);
```

> 旧代码如果调用 `lock_screen_apply_error_face()`，它仍然可用：
> 这是保留的兼容接口，内部直接转调用 `cry_face_replace_error()`。

## 4. 行为参数

### 数字雨

| 参数 | 值 |
| --- | --- |
| 粒子数 | 8（按奇偶分到上下两行） |
| 生成列 | 0~14 |
| 移动方向 | 严格从左向右（每拍 x+1） |
| 速度 | `90 + 数字×22 + 随机(0~29)` 毫秒/格 |
| 消失位置 | 生成列右侧的随机终点 |
| 解锁 | 锁屏中检测到 MODE(T2) |
| 重新上锁 | 计算中"干净的单键 MODE"（stable_bitmap 只有 T2） |
| 防误锁 | MODE+← 串扰（{T2,T7}）不触发锁屏 |

### 哭脸

| 参数 | 值 |
| --- | --- |
| CGRAM 槽位 | 2（`LCD1602_CHAR_ANGLE=1` 旁边） |
| 点阵 | 8 行 × 5 列（两只眼睛 + 下弯的嘴） |
| 替换规则 | 在 16 字符行内查找 `ERROR`，命中后清空该行并把哭脸放在原 `E` 的位置 |

## 5. 主机验证

```powershell
cd Comsen_解读\code\tests
zig cc -std=c11 -I .\stubs -I ..\src\include -o test_innovation.exe ^
       test_innovation.c .\stubs\stm32f1xx_hal.c ^
       ..\src\lock_screen.c ..\src\cry_face.c ..\src\lcd1602.c ..\src\bsp_delay.c
.\test_innovation.exe
```

结果：

```
==== 创新模块测试 ====
[1] 哭脸报错
[2] 数字雨锁屏
==== 通过 18 项，失败 0 项 ====
```

测试覆盖：`ERROR` 替换位置、`READY` 不受影响、锁屏默认状态、MODE 解锁、
MODE+← 串扰不误锁、干净 MODE 重新上锁、数字雨运行不越界。
