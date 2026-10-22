#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

/*
 * 本板接线总表 —— 全部来自 Gerber 网表（FlyingProbeTesting.json）+ software.ioc
 *
 * 排针 H1/H2 = STM32 核心板插座，H3 = 1602 液晶的 16Pin 排针。
 * 只用到 PB6~PB15，其余引脚未引出到外设。
 *
 *   H3-1  VSS  -> GND                 H3-9  D2  -> 悬空(NET_29)
 *   H3-2  VDD  -> 3.3V                H3-10 D3  -> 悬空(NET_30)
 *   H3-3  V0   -> R11/R12 分压         H3-11 D4  -> PB12
 *   H3-4  RS   -> PB10                H3-12 D5  -> PB13
 *   H3-5  RW   -> GND（只写）          H3-13 D6  -> PB14
 *   H3-6  E    -> PB11                H3-14 D7  -> PB15
 *   H3-7  D0   -> 悬空(NET_27)         H3-15 A   -> R13 -> 3.3V（背光常亮）
 *   H3-8  D1   -> 悬空(NET_28)         H3-16 K   -> GND
 *
 *   触摸芯片 U1: SCL=PB6, SDO=PB7, 负责 T0 ~ T14（板上半部分 3 行）
 *   触摸芯片 U2: SCL=PB8, SDO=PB9, 负责 T15 ~ T29（板下半部分 3 行）
 *   测试点:  TP2=PB7  TP3=PB6  TP4=PB9  TP5=PB8  TP1=GND
 *   心跳灯:  PC13（核心板自带的 LED，板上没有额外走线）
 */

#include "stm32f1xx_hal.h"

/* ---------------- 液晶引脚（H3 排针） ---------------- */
#define LCD_RS_PORT        GPIOB
#define LCD_RS_PIN         GPIO_PIN_10
#define LCD_EN_PORT        GPIOB
#define LCD_EN_PIN         GPIO_PIN_11
#define LCD_D4_PORT        GPIOB
#define LCD_D4_PIN         GPIO_PIN_12
#define LCD_D5_PORT        GPIOB
#define LCD_D5_PIN         GPIO_PIN_13
#define LCD_D6_PORT        GPIOB
#define LCD_D6_PIN         GPIO_PIN_14
#define LCD_D7_PORT        GPIOB
#define LCD_D7_PIN         GPIO_PIN_15

/* ---------------- 触摸芯片引脚 ---------------- */
#define TOUCH_A_SCL_PORT   GPIOB        /* U1 时钟，接 PB6 */
#define TOUCH_A_SCL_PIN    GPIO_PIN_6
#define TOUCH_A_SDO_PORT   GPIOB        /* U1 数据，接 PB7 */
#define TOUCH_A_SDO_PIN    GPIO_PIN_7

#define TOUCH_B_SCL_PORT   GPIOB        /* U2 时钟，接 PB8 */
#define TOUCH_B_SCL_PIN    GPIO_PIN_8
#define TOUCH_B_SDO_PORT   GPIOB        /* U2 数据，接 PB9 */
#define TOUCH_B_SDO_PIN    GPIO_PIN_9

/* ---------------- 心跳灯 ---------------- */
#define HEARTBEAT_LED_PORT GPIOC
#define HEARTBEAT_LED_PIN  GPIO_PIN_13

/* ---------------- 时序参数（单位 us / ms） ---------------- */
#define TOUCH_CLK_HALF_US  3U     /* 时钟高/低电平各保持 3us，TTP229 两线时序典型值 */
#define TOUCH_IDLE_US      10U    /* 两次扫描之间让芯片休息（配合 10ms 扫描周期） */
#define LCD_EN_PULSE_US    2U     /* 液晶 EN 脉宽（>450ns 即可） */
#define LCD_CMD_SETTLE_US  50U    /* 普通命令后的等待 */
#define LCD_CLEAR_MS       2U     /* 清屏/回原点命令需要较长等待 */

#endif /* BOARD_CONFIG_H */
