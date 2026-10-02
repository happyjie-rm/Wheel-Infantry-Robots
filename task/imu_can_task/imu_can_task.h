#ifndef IMU_CAN_TASK_H
#define IMU_CAN_TASK_H
#include "comp_cmd.h"
#ifdef __cplusplus
extern "C" {
#endif

/* IMU 对象由 can_device 模块注册，任务通过 getter 访问。 */
void imu_can_task(void* argument);
#ifdef __cplusplus
}
#endif
#endif
