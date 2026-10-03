/**
 * @file shoot_control.h
 * @brief 发射机构控制接口 (VT13 遥控器)
 */
#pragma once

#include "comp_cmd.h"

/**
 * @brief 初始化发射机构总线与电机（须在 CAN Start 之前调用）
 * @return OK 成功；其他为错误码
 */
err_t shoot_control_init(void);

/**
 * @brief 初始化发射电机 PID 参数
 * @details 摩擦轮 M3508 单速度环；拨弹 M2006 速度位置双环
 */
void shoot_speed_pid_init(void);

/**
 * @brief 发射模式控制主函数
 * @details 根据遥控器状态执行对应的发射控制逻辑
 */
void Shoot_Mode(void);
