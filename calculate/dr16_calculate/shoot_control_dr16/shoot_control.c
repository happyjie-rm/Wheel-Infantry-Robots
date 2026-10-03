/**
 * @file shoot_control.c
 * @brief 发射机构控制实现 (DR16 遥控器)
 */
#include "shoot_control.h"

#include <math.h>
#include <stdbool.h>

#include "bsp_can.h"
#include "can.h"
#include "dj_motor_ctrl.h"
#include "dr16.h"
#include "pid_incremental.h"
#include "process.h"

/* DR16 task owns the decoded command and updates it periodically. */
extern DR16_t *dr16;

/* 发射机构电机总线与实例 */
static dj_motor_bus_t shoot_bus;
static dj_motor_t shoot_motors[3];
static err_t shoot_bus_init_result;
static err_t shoot_init_result[3];
/* 速度环 PID 实例：三台电机各一个 */
static PIDInstance_jie pid_speed[3];
/* 位置环 PID 实例：仅拨弹电机使用速度位置双环 */
static PIDInstance_jie pid_location;

/**
 * @brief 初始化发射 CAN 总线与三台发射电机
 * @return OK 或错误码
 */
err_t shoot_control_init(void) {
  /* 获取 CAN1 的 BSP 对象（假设 CAN1 用于发射机构，与云台同总线）
   * CAN2 上底盘已占用 0x200 控制组的 1-4 号电机，发射机构不能复用同组 ID */
  BSP_CAN_t can_id = BSP_CAN_get_id(CAN1);
  if (can_id == BSP_CAN_ID_ERROR) {
    return NOT_FOUND;
  }

  STM32CAN_t *can1 = STM32CAN_GetInstance(can_id);
  if (can1 == NULL) {
    return PTR_NULL;
  }

  /* 初始化发射机构总线 */
  shoot_bus_init_result = dj_motor_bus_init(&shoot_bus, can1);
  if (shoot_bus_init_result != OK) {
    return shoot_bus_init_result;
  }

  /* 初始化发射机构电机（控制组 0x200）
   * 实车电机编号与机构的对应关系：
   *   左摩擦轮 = 1 号电机（M3508，单速度环）
   *   右摩擦轮 = 2 号电机（M3508，单速度环）
   *   拨弹电机 = 3 号电机（M2006，速度位置双环）
   * reversed 参数根据实际机械安装方向设置 */
  shoot_init_result[0] =
      dj_motor_init(&shoot_motors[0], &shoot_bus, DJ_MOTOR_M3508, 1,
                    false); /* 左摩擦轮 = 1 号 */
  if (shoot_init_result[0] != OK)
    return shoot_init_result[0];

  shoot_init_result[1] =
      dj_motor_init(&shoot_motors[1], &shoot_bus, DJ_MOTOR_M3508, 2,
                    false); /* 右摩擦轮 = 2 号 */
  if (shoot_init_result[1] != OK)
    return shoot_init_result[1];

  shoot_init_result[2] =
      dj_motor_init(&shoot_motors[2], &shoot_bus, DJ_MOTOR_M2006, 3,
                    false); /* 拨弹电机 = 3 号 */
  if (shoot_init_result[2] != OK)
    return shoot_init_result[2];

  return OK;
}

void shoot_speed_pid_init(void) {
  // 初始化左摩擦轮电机速度 PID 参数（M3508 单速度环）
  PID_Init_Params_jie(&pid_speed[0], 10.0f, 0.0f, 0.0f, 0.0002f, 12000.0f,
                      3000.0f, 0.0f,
                      PID_Integral_Limit | PID_Derivative_On_Measurement |
                          PID_OutputFilter | PID_DerivativeFilter);
  // 初始化右摩擦轮电机速度 PID 参数（M3508 单速度环）
  PID_Init_Params_jie(&pid_speed[1], 10.0f, 0.0f, 0.0f, 0.0002f, 12000.0f,
                      3000.0f, 0.0f,
                      PID_Integral_Limit | PID_Derivative_On_Measurement |
                          PID_OutputFilter | PID_DerivativeFilter);
  // 初始化拨弹电机速度环 PID 参数（M2006 双环内环，输出电流限幅 10000）
  PID_Init_Params_jie(&pid_speed[2], 8.0f, 0.0f, 0.0f, 0.0002f, 10000.0f,
                      3000.0f, 0.0f,
                      PID_Integral_Limit | PID_Derivative_On_Measurement |
                          PID_OutputFilter | PID_DerivativeFilter);
  // 初始化拨弹电机位置环 PID 参数（M2006 双环外环，输出为目标速度）
  PID_Init_Params_jie(&pid_location, 8.0f, 0.0f, 0.0f, 0.0002f, 3000.0f,
                      1000.0f, 0.0f,
                      PID_Integral_Limit | PID_Derivative_On_Measurement |
                          PID_OutputFilter | PID_DerivativeFilter);
}

void Shoot_Mode(void) {
  // TODO: 实现发射模式控制逻辑
}
