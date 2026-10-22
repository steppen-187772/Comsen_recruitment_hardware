# STM32 触摸计算器 — 提交工程

[![CI](https://github.com/steppen-187772/Comsen_recruitment_hardware/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/steppen-187772/Comsen_recruitment_hardware/actions/workflows/ci.yml)

- 姓名：孔紫仪
- 年级：大一
- 专业：光学与电子信息
- 完成日期：2026-10-18

## 1. 最终固件配置

采用「保三换三」：

| 模块 | 实现 |
| --- | --- |
| `calculator_engine` | 自研（表达式、复数、错误码、结果格式化） |
| `touch_filter` | 自研（逐位消抖、单次上报） |
| `lcd1602` | 自研（4 位驱动、CGRAM 自定义字符） |
| `ttp229` | 官方 `libttp229.a` |
| `touch_model` | 官方 `libtouch_model.a` |
| `calculator_app` | 官方 `libcalculator_app.a` |

自研的 `ttp229` / `touch_model` / 应用层版本在联调中出现过读取不稳定、
按键错位和任务栈问题，最终为保证交付固件稳定，保留官方实现；
相关过程记录在工程外部的测试文档中。

## 2. 目录结构

```
submission/
├── README.md              ← 本文件
├── REPORT.md              ← 报告（提交前请确认是本人最终版本）
├── video/                 ← 视频说明；高清视频见Releases
├── innovations/           ← 创新模块：数字雨锁屏 + 哭脸报错（不参与主固件构建）
├── CMakeLists.txt
├── CMakePresets.json
├── Core/                  ← CubeMX 生成代码 + 自研模块
├── Drivers/               ← ST HAL / CMSIS
├── Middlewares/           ← FreeRTOS / USB
├── USB_DEVICE/            ← USB 设备库（主固件默认不启动）
├── lib/                   ← 官方预编译库
├── software.ioc           ← CubeMX 配置
├── startup_stm32f103xb.s
└── stm32f103c8tx_flash.ld
```

## 3. 编译

需要 Arm GNU Toolchain（STM32CubeIDE 自带即可）、CMake 3.22+、Ninja。

```bash
cmake -S . -B build/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/Release
```

产物：`build/Release/software.elf / .hex / .bin`。

## 4. 烧录

用 ST-Link（SWD）和 STM32CubeProgrammer：

```bash
STM32_Programmer_CLI -c port=SWD -w build/Release/software.hex 0x08000000 -v -rst
```

## 5. 已实现功能

- 1602 显示（4 位模式、两行 16 字符）；
- 30 键触摸输入（官方 TTP229 + 滤波 + 识别）；
- 四则运算与 AC；
- 运算符优先级与括号（由自研计算引擎支持）；
- 语法错 / 除零 / 定义域错误提示；
- 结果格式化（16 字符内自动定点/科学计数法）。

说明：USB CDC 虚拟串口在独立试验固件中做了准备，
受 Type-C 数据线/接口限制未完成最终验证，因此没有并入本提交的主固件。

## 6. 验证

- 主机测试（虚拟 1602 + 假触摸芯片）：59 项断言通过；
- 单模块替换测试：每次只替换一个 `.a`，其余保持官方；
- 烧录校验：STM32CubeProgrammer 下载校验 + 板上回读比对；
- 最终固件：Flash 43,856 B / 64KB（66.92%），RAM 13,176 B / 20KB（64.34%）。

## 7. 创新模块（独立）

`innovations/` 中提供两个独立模块：

- 数字雨锁屏（`lock_screen.c/.h`）；
- 哭脸报错（`cry_face.c/.h`，用 CGRAM 字符替代 `ERROR`）。

它们来自另一个试验固件，**不参与本计算器主工程的构建**，
接入方法见 `innovations/README_创新模块说明.md`。

## 8. 验收视频

高清验收视频已作为 GitHub Release 附件提交。Release 标题为
`Comsen招新-硬件组-大一-孔紫仪`；GitHub 会把中文附件名自动转义为
`Comsen.-.-.-.mp4`：

[观看或下载验收视频](https://github.com/steppen-187772/Comsen_recruitment_hardware/releases/tag/submission-v1)
