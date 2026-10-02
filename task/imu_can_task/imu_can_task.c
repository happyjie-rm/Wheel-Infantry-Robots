#include "imu_can_task.h"

#include "FreeRTOS.h"
#include "can_device.h"
#include "comp_utils.h"
#include "imu_can.h"
#include "task.h"

void imu_can_task(void* argument) {
  RM_UNUSED(argument);
  IMUCAN_t* imu = can_device_get_imu();
  if (imu->init_error_ != OK) {
    vTaskDelete(NULL);
    return;
  }

  /* 设备已由 can_device 模块注册；任务只绑定自己的通知句柄并消费数据。 */
  imu->thread_alert = xTaskGetCurrentTaskHandle();
  for (;;) imu_can_update(imu, 2u);
}
