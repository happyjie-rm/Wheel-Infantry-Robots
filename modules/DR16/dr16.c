#include "dr16.h"

#include <string.h>

#include "comp_utils.h"

// ==================== DMA 缓冲与对象状态 ====================

static uint8_t dma_buffer_[DR16_FRAME_SIZE * 2];
static uint8_t dr16_rx_buffer_[DR16_FRAME_SIZE];
static volatile bool frame_ready_ = false;
static DR16_t *instance_ = NULL;

// ==================== 原始帧校验与解析 ====================

//! 检查 DR16 原始通道值和拨杆值是否落在协议合法范围内。
static bool DR16_DataCorrupted(const dr16_data_t *dr16)
{
  ASSERT(dr16);

  if ((dr16->ch_r_x < DR16_CH_VALUE_MIN) || (dr16->ch_r_x > DR16_CH_VALUE_MAX))
    return true;

  if ((dr16->ch_r_y < DR16_CH_VALUE_MIN) || (dr16->ch_r_y > DR16_CH_VALUE_MAX))
    return true;

  if ((dr16->ch_l_x < DR16_CH_VALUE_MIN) || (dr16->ch_l_x > DR16_CH_VALUE_MAX))
    return true;

  if ((dr16->ch_l_y < DR16_CH_VALUE_MIN) || (dr16->ch_l_y > DR16_CH_VALUE_MAX))
    return true;

  if (dr16->sw_l == CMD_SW_ERR)
    return true;

  if (dr16->sw_r == CMD_SW_ERR)
    return true;

  return false;
}

//! 将 18 字节原始帧解析为通用 cmd_rc_t，并把摇杆通道归一化到约 -1~1。
static err_t DR16_ParseRc(const dr16_data_t *dr16, cmd_rc_t *rc)
{
  ASSERT(dr16);
  ASSERT(rc);

  if ((dr16 == NULL) || (rc == NULL)) {
    return PTR_NULL;
  }

  if (DR16_DataCorrupted(dr16)) {
    return FAILED;
  }

  memset(rc, 0, sizeof(*rc));

  const float full_range = (float)(DR16_CH_VALUE_MAX - DR16_CH_VALUE_MIN);  // 归一化分母。

  rc->ch.r.x = 2.0f * ((float)dr16->ch_r_x - DR16_CH_VALUE_MID) / full_range;
  rc->ch.r.y = 2.0f * ((float)dr16->ch_r_y - DR16_CH_VALUE_MID) / full_range;
  rc->ch.l.x = 2.0f * ((float)dr16->ch_l_x - DR16_CH_VALUE_MID) / full_range;
  rc->ch.l.y = 2.0f * ((float)dr16->ch_l_y - DR16_CH_VALUE_MID) / full_range;

  rc->sw_l = (cmd_switch_pos_t)dr16->sw_l;
  rc->sw_r = (cmd_switch_pos_t)dr16->sw_r;

  rc->mouse.x = dr16->x;
  rc->mouse.y = dr16->y;
  rc->mouse.z = dr16->z;
  rc->mouse.click.l = dr16->press_l;
  rc->mouse.click.r = dr16->press_r;

  rc->key = dr16->key;
  rc->ch_res = ((float)dr16->res - DR16_CH_VALUE_MID) / full_range;

  return OK;
}

// ==================== ISR 接收入口 ====================

//! RX DMA 接收回调（中断上下文）：只接受完整 DR16 帧并通知任务解析。
static void DR16_RxCallback(uint8_t *data, size_t size)
{
  if (size == DR16_FRAME_SIZE) {
    memcpy(dr16_rx_buffer_, data, DR16_FRAME_SIZE);  // DMA 会继续写原缓冲，先快照一份。
    frame_ready_ = true;

    if ((instance_ != NULL) && (instance_->thread_alert != NULL)) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      xTaskNotifyFromISR(instance_->thread_alert, SIGNAL_DR16_RAW_REDY,
                         eSetBits, &xHigherPriorityTaskWoken);
      portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
  }
}

// ==================== 初始化与任务周期处理 ====================

//! 初始化 DR16 对象，绑定 UART RX DMA 和接收回调。
err_t DR16_Init(DR16_t *self, UART_HandleTypeDef *uart_handle)
{
  if (self == NULL) {
    return PTR_NULL;
  }

  memset(self, 0, sizeof(*self));
  self->switch_required = pdFALSE;
  self->online_ = false;
  self->init_error_ = STM32UART_Init(
      &self->uart_, uart_handle,
      (BSP_UART_RawData_t){dma_buffer_, sizeof(dma_buffer_)}, DR16_RxCallback);
  instance_ = self;
  return self->init_error_;
}

//! 启动 DR16 UART DMA 接收。
err_t DR16_Start(DR16_t *self)
{
  if (self == NULL) {
    return PTR_NULL;
  }
  if (self->init_error_ != OK) {
    return self->init_error_;
  }
  return STM32UART_SetRxDMA(&self->uart_);
}

//! 等待一帧 DR16 数据；超时或解析失败时清零命令并标记离线。
void DR16_Update(DR16_t *self, uint32_t timeout_ms)
{
  if (self == NULL) {
    return;
  }

  uint32_t notify_value = 0;
  BaseType_t result = xTaskNotifyWait(SIGNAL_DR16_RAW_REDY, UINT32_MAX,
                                      &notify_value, pdMS_TO_TICKS(timeout_ms));

  if ((result == pdTRUE) && frame_ready_) {
    frame_ready_ = false;
    memcpy(&self->dr16_data_, dr16_rx_buffer_, DR16_FRAME_SIZE);  // 使用快照解析，避免 DMA 并发改写。

    if (DR16_ParseRc(&self->dr16_data_, &self->dr16_cmd) == OK) {
      self->online_ = true;
    }
  } else {
    self->online_ = false;
    memset(&self->dr16_cmd, 0, sizeof(self->dr16_cmd));
  }
}
