/**
 * @file shoot_task.h
 * @brief 发射控制任务头文件
 */
#pragma once

#include "comp_cmd.h"

/**
 * @brief 发射任务状态
 * @details 记录发射任务最近一次的初始化或控制状态
 */
extern volatile err_t shoot_status;

/**
 * @brief 发射任务入口函数
 * @param argument FreeRTOS 任务参数（未使用）
 */
void shoot_task(void *argument);
