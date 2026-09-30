#include "vofa.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "comp_utils.h"

// ==================== 参数表 ====================

//! VOFA ASCII 命令参数缓存项。
typedef struct {
  const char *name;
  float value;
  bool received;
} VofaParameter_t;

static VofaParameter_t vofa_parameters[] = {
    {"speed", 0.0f, false},
    {"angle", 0.0f, false},
    {"mit", 0.0f, false},
};

static uint8_t vofa_tx_buff_[2][VOFA_TX_LEN];
static uint8_t vofa_rx_dma_buff_[VOFA_RX_DMA_BUF_LEN];
static uint8_t vofa_rx_fifo_[VOFA_RX_LEN];
static char vofa_cmd_buff_[VOFA_RX_CMD_BUF_LEN];
static const uint8_t kVofaTail[VOFA_TAIL_LEN] = {0x00, 0x00, 0x80, 0x7f};

static volatile uint16_t rx_head_ = 0U;
static volatile uint16_t rx_tail_ = 0U;
static volatile bool rx_fifo_overflowed_ = false;
static uint32_t rx_fifo_overflow_count_ = 0U;

static uint32_t cmd_overflow_count_ = 0U;
static uint16_t cmd_len_ = 0U;
static bool cmd_overflowed_ = false;

static Vofa_t *instance_ = NULL;

// ==================== FIFO 索引与字符校验工具 ====================

//! 计算环形 FIFO 下一个索引；VOFA_RX_LEN 必须为 2 的幂。
static uint16_t Vofa_NextRxIndex(uint16_t index)
{
  return (uint16_t)((index + 1U) & (VOFA_RX_LEN - 1U));
}

//! 命令名必须以字母或下划线开头。
static bool IsCommandNameStart(char ch)
{
  return ((ch >= 'A') && (ch <= 'Z')) || ((ch >= 'a') && (ch <= 'z')) ||
         (ch == '_');
}

//! 命令名后续字符允许字母、数字或下划线。
static bool IsCommandNameChar(char ch)
{
  return IsCommandNameStart(ch) || ((ch >= '0') && (ch <= '9'));
}

//! 检查命令名是否合法，避免把异常输入写入参数表。
static bool IsCommandNameValid(const char *name)
{
  if ((name == NULL) || (name[0] == '\0') || !IsCommandNameStart(name[0])) {
    return false;
  }
  for (size_t i = 1U; name[i] != '\0'; ++i) {
    if (!IsCommandNameChar(name[i])) {
      return false;
    }
  }
  return true;
}

//! 命令值不允许出现空白字符，便于保持 name=value! 格式简单明确。
static bool HasWhitespace(const char *text)
{
  for (size_t i = 0U; text[i] != '\0'; ++i) {
    const char ch = text[i];
    if ((ch == ' ') || (ch == '\t') || (ch == '\r') || (ch == '\n')) {
      return true;
    }
  }
  return false;
}

// ==================== 参数表操作 ====================

//! 查找参数表项，不存在则返回 NULL。
static VofaParameter_t *FindParameter(const char *name)
{
  if (name == NULL) {
    return NULL;
  }

  for (size_t i = 0U; i < ARRAY_LEN(vofa_parameters); ++i) {
    if (strcmp(vofa_parameters[i].name, name) == 0) {
      return &vofa_parameters[i];
    }
  }
  return NULL;
}

//! 设置参数值，并标记该参数已经收到过。
static void SetParameter(const char *name, float value)
{
  VofaParameter_t *parameter = FindParameter(name);
  if (parameter == NULL) {
    return;
  }

  taskENTER_CRITICAL();
  parameter->value = value;      // 更新参数值。
  parameter->received = true;    // 标记参数已收到过。
  taskEXIT_CRITICAL();
}

// ==================== ISR 到任务环形 FIFO ====================

//! 从 ISR 写入接收 FIFO，满时标记溢出并丢弃当前字节。
static void Vofa_PushRxByteFromISR(uint8_t byte)
{
  const uint16_t head = rx_head_;
  const uint16_t next_head = Vofa_NextRxIndex(head);

  if (next_head == rx_tail_) {
    rx_fifo_overflowed_ = true;  // 留一个空位区分 FIFO 空/满。
    ++rx_fifo_overflow_count_;
    return;
  }

  vofa_rx_fifo_[head] = byte;
  rx_head_ = next_head;
}

//! 在任务上下文批量取出 FIFO 数据，返回是否取到数据。
static bool Vofa_PopRxBatch(uint8_t *buf, uint16_t *count)
{
  *count = 0U;
  taskENTER_CRITICAL();
  while (rx_tail_ != rx_head_) {
    buf[*count] = vofa_rx_fifo_[rx_tail_];
    rx_tail_ = Vofa_NextRxIndex(rx_tail_);
    ++(*count);
  }
  taskEXIT_CRITICAL();
  return (*count > 0U);
}

//! 消费并清除 RX FIFO 溢出标志。
static bool Vofa_ConsumeRxOverflow(void)
{
  taskENTER_CRITICAL();
  const bool overflowed = rx_fifo_overflowed_;
  rx_fifo_overflowed_ = false;
  taskEXIT_CRITICAL();
  return overflowed;
}

//! 清空软件 FIFO：直接追平 tail 到当前 head。
static void Vofa_ClearRxFifo(void)
{
  taskENTER_CRITICAL();
  rx_tail_ = rx_head_;
  taskEXIT_CRITICAL();
}

// ==================== ASCII 命令解析 ====================

//! 重置命令累积缓冲状态。
static void Vofa_ResetCommand(void)
{
  cmd_len_ = 0U;
  cmd_overflowed_ = false;
}

//! 解析命令缓冲区中的 name=value! 命令，并更新参数表。
static err_t Vofa_ParseCommand(void)
{
  if (cmd_len_ == 0U) {
    return FAILED;
  }

  vofa_cmd_buff_[cmd_len_] = '\0';  // 添加字符串结束符，供 strtof/strcmp 使用。
  char *separator = (char *)memchr(vofa_cmd_buff_, '=', (size_t)cmd_len_);  // 查找 name/value 分隔符。
  if ((separator == NULL) || (separator == vofa_cmd_buff_) ||
      (separator == &vofa_cmd_buff_[cmd_len_ - 1U])) {
    return FAILED;
  }

  *separator = '\0';
  const char *name = vofa_cmd_buff_;
  const char *value_text = separator + 1;

  if (!IsCommandNameValid(name) || HasWhitespace(value_text)) {
    return FAILED;
  }

  char *value_end = NULL;
  const float value = strtof(value_text, &value_end);
  if ((value_end == value_text) || (value_end == NULL) ||
      (*value_end != '\0') || !isfinite(value)) {
    return FAILED;
  }

  SetParameter(name, value);
  if ((instance_ != NULL) && (instance_->command_callback_ != NULL)) {
    instance_->command_callback_(name, value);
  }

  return OK;
}

//! 逐字节累积命令，遇到 ! 作为命令结束符；CR/LF 被忽略。
static void Vofa_ProcessRxByte(uint8_t byte)
{
  if ((byte == '\r') || (byte == '\n')) {
    return;
  }

  if (byte == '!') {  // VOFA 下行命令结束符。
    if (!cmd_overflowed_ && (cmd_len_ > 0U)) {
      (void)Vofa_ParseCommand();
    }
    Vofa_ResetCommand();
    return;
  }

  if (cmd_overflowed_) {
    return;
  }

  if (cmd_len_ >= (VOFA_RX_CMD_BUF_LEN - 1U)) {
    cmd_overflowed_ = true;  // 当前命令过长，丢弃到下一个 !。
    ++cmd_overflow_count_;
    cmd_len_ = 0U;
    return;
  }

  vofa_cmd_buff_[cmd_len_] = (char)byte;
  ++cmd_len_;
}

// ==================== ISR 接收入口 ====================

//! RX DMA 接收回调（中断上下文）：写入 FIFO 后通知任务解析。
static void Vofa_RxCallback(uint8_t *data, size_t size)
{
  if ((data == NULL) || (size == 0U)) {
    return;
  }

  for (size_t i = 0U; i < size; ++i) {
    Vofa_PushRxByteFromISR(data[i]);
  }

  if ((instance_ != NULL) && (instance_->thread_alert != NULL)) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(instance_->thread_alert, VOFA_SIGNAL_RAW_READY, eSetBits,
                       &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

// ==================== 初始化与任务周期处理 ====================

//! 初始化 VOFA 对象，绑定 UART 收发通道和静态 DMA 缓冲。
err_t Vofa_Init(Vofa_t *self, UART_HandleTypeDef *uart_handle)
{
  if (self == NULL) {
    return PTR_NULL;
  }

  memset(self, 0, sizeof(*self));
  self->tx_init_error_ = STM32UARTFrameTx_Init(
      &self->uart_send_, uart_handle,
      (BSP_UART_RawData_t){vofa_tx_buff_[0], sizeof(vofa_tx_buff_[0])},
      (BSP_UART_RawData_t){vofa_tx_buff_[1], sizeof(vofa_tx_buff_[1])},
      NULL);
  self->rx_init_error_ = STM32UART_Init(
      &self->uart_receive_, uart_handle,
      (BSP_UART_RawData_t){vofa_rx_dma_buff_, sizeof(vofa_rx_dma_buff_)},
      Vofa_RxCallback);
  instance_ = self;

  return (self->tx_init_error_ != OK) ? self->tx_init_error_ : self->rx_init_error_;
}

//! 配置变长帧 TX DMA 并启动 RX 循环 DMA；只发送实际帧长。
err_t Vofa_Start(Vofa_t *self)
{
  if (self == NULL) {
    return PTR_NULL;
  }
  if (self->tx_init_error_ != OK) {
    return self->tx_init_error_;
  }
  if (self->rx_init_error_ != OK) {
    return self->rx_init_error_;
  }

  err_t err = STM32UARTFrameTx_SetTxDMA(&self->uart_send_);
  if (err != OK) {
    return err;
  }
  return STM32UART_SetRxDMA(&self->uart_receive_);
}

//! 任务周期入口：处理溢出恢复、排空 FIFO，并解析 ASCII 命令。
void Vofa_Update(Vofa_t *self, uint32_t timeout_ms)
{
  if (self == NULL) {
    return;
  }

  xTaskNotifyWait(0U, VOFA_SIGNAL_RAW_READY, NULL, pdMS_TO_TICKS(timeout_ms));

  if (Vofa_ConsumeRxOverflow()) {
    Vofa_ClearRxFifo();
    Vofa_ResetCommand();
  }

  uint8_t local_buf[VOFA_RX_LEN];
  uint16_t count = 0U;
  Vofa_PopRxBatch(local_buf, &count);

  for (uint16_t i = 0U; i < count; ++i) {
    Vofa_ProcessRxByte(local_buf[i]);  // 逐字节推进命令解析。
  }

  if (Vofa_ConsumeRxOverflow()) {
    Vofa_ClearRxFifo();
    Vofa_ResetCommand();
  }
}

// ==================== 公有接口 ====================

//! 设置命令回调函数。
void Vofa_SetCommandCallback(Vofa_t *self, Vofa_CommandCallback_t callback)
{
  if (self == NULL) {
    return;
  }
  self->command_callback_ = callback;
}

//! 获取参数值：参数存在且已收到过则返回 true。
bool Vofa_GetParameter(const char *name, float *value)
{
  if (value == NULL) {
    return false;
  }

  VofaParameter_t *parameter = FindParameter(name);
  if (parameter == NULL) {
    return false;
  }

  taskENTER_CRITICAL();
  const bool received = parameter->received;  // 只在收到过有效命令后输出参数值。
  if (received) {
    *value = parameter->value;
  }
  taskEXIT_CRITICAL();

  return received;
}

//! 获取 speed 参数。
bool Vofa_GetSpeed(float *speed)
{
  return Vofa_GetParameter("speed", speed);
}

//! 获取 angle 参数。
bool Vofa_GetAngle(float *angle)
{
  return Vofa_GetParameter("angle", angle);
}

//! 发送 VOFA firewater 数据帧，每个 float 占 4 字节，尾部追加 00 00 80 7F。
err_t Vofa_Send(Vofa_t *self, const float *data, size_t size)
{
  if ((self == NULL) || (data == NULL)) {
    return PTR_NULL;
  }
  if ((size == 0U) || (size > VOFA_MAX_FLOATS)) {
    return FAILED;
  }

  const size_t payload_len = size * sizeof(float);      // 纯 float 数据长度。
  const size_t frame_len = payload_len + VOFA_TAIL_LEN; // 完整 firewater 帧长度。
  uint8_t frame[VOFA_TX_LEN];
  memcpy(frame, data, payload_len);
  memcpy(frame + payload_len, kVofaTail, VOFA_TAIL_LEN);

  // BSP 内部保护任务/ISR 共享状态；两帧均占用时返回 BUSY，交给调用方重试。
  return STM32UARTFrameTx_Write(&self->uart_send_, frame, frame_len);
}
