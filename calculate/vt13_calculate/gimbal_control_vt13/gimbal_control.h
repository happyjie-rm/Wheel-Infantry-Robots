/**
 * @file gimbal_control.h
 * @brief 云台控制接口 (VT13 遥控器)
 */
#pragma once

/**
 * @brief 初始化云台电机速度环 PID 参数
 */
void gimbal_speed_pid_init(void);

/**
 * @brief 云台模式控制主函数
 * @details 根据遥控器状态执行对应的云台控制逻辑
 */
void Gimbal_Mode(void);
