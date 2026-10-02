/**
 * @file gimbal_task.h
 * @brief 云台控制任务头文件
 */
#pragma once

#include "comp_cmd.h"

/**
 * @brief 云台任务状态
 * @details 记录云台任务最近一次的初始化或控制状态
 */
extern volatile err_t gimbal_status;

/**
 * @brief 云台任务入口函数
 * @param argument FreeRTOS 任务参数（未使用）
 */
void gimbal_task(void *argument);
