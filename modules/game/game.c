// 游戏遥控接收模块 - 使用 UART BSP 循环 DMA 接收裁判系统字节流

#include "game.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "comp_utils.h"
#include "crc_ref.h"

#define GAME_RX_BATCH_LEN GAME_RX_DMA_BUF_SIZE

CommuniCateTypeDef custom_robot_data;

static uint8_t game_rx_dma_buff_[GAME_RX_DMA_BUF_SIZE];
static Game_t *instance_ = NULL;

/**
 * @brief 计算遥控器数据帧的总长度
 *
 * 此函数根据数据帧头部的信息计算整个数据帧的长度，
 * 包括帧头、有效载荷、命令ID和帧尾的总和。
 *
 * @param frame 指向数据帧起始位置的指针
 * @return size_t 返回计算得出的数据帧总长度
 *
 * @note 数据帧格式为：帧头(HEAD) + 数据长度字段 + 命令ID + 有效载荷 + 帧尾(TAIL)
 *       其中数据长度字段指示了有效载荷的长度
 */
static size_t Game_GetFrameLen(const uint8_t *frame)
{
  // 从数据帧中提取有效载荷长度信息
  // 数据长度字段是两个字节，低位在前，高位在后
  uint16_t payload_len =
      (uint16_t)(frame[DATA_LENGTH] | // 低字节
                 ((uint16_t)frame[DATA_LENGTH + 1U] << 8U)); // 高字节左移8位
  // 计算总帧长 = 有效载荷长度 + 帧头长度 + 命令ID长度 + 帧尾长度
  return (size_t)payload_len + Frame_header_LEN + CMD_ID_LEN +
         Frame_tail_LEN;
}

//! 计算环形 FIFO 下一个索引。
static uint16_t Game_NextRxIndex(uint16_t index)
{
  ++index;
  if (index >= GAME_RX_FIFO_LEN) {
    index = 0U;
  }
  return index;
}

//! 重置半包/粘包暂存状态。
static void Game_ResetStash(Game_t *self)
{
  if (self == NULL) {
    return;
  }
  self->stash_len = 0U;
}

//! 从 ISR 写入接收 FIFO，满时标记溢出并丢弃当前字节。
static void Game_PushRxByteFromISR(Game_t *self, uint8_t byte)
{
  if (self == NULL) {
    return;
  }

  const uint16_t head = self->rx_head;
  const uint16_t next_head = Game_NextRxIndex(head);

  if (next_head == self->rx_tail) {
    self->rx_fifo_overflowed = true;
    ++self->rx_fifo_overflow_count;
    return;
  }

  self->rx_fifo[head] = byte;
  self->rx_head = next_head;
}

//! 在任务上下文批量取出 FIFO 数据，返回是否取到数据。
static bool Game_PopRxBatch(Game_t *self, uint8_t *buf, uint16_t capacity,
                            uint16_t *count)
{
  if ((self == NULL) || (buf == NULL) || (count == NULL) ||
      (capacity == 0U)) {
    return false;
  }

  *count = 0U;
  taskENTER_CRITICAL();
  while ((self->rx_tail != self->rx_head) && (*count < capacity)) {
    buf[*count] = self->rx_fifo[self->rx_tail];
    self->rx_tail = Game_NextRxIndex(self->rx_tail);
    ++(*count);
  }
  taskEXIT_CRITICAL();

  return (*count > 0U);
}

//! 消费并清除 RX FIFO 溢出标志。
static bool Game_ConsumeRxOverflow(Game_t *self)
{
  if (self == NULL) {
    return false;
  }

  taskENTER_CRITICAL();
  const bool overflowed = self->rx_fifo_overflowed;
  self->rx_fifo_overflowed = false;
  taskEXIT_CRITICAL();
  return overflowed;
}

//! 清空软件 FIFO：直接追平 tail 到当前 head。
static void Game_ClearRxFifo(Game_t *self)
{
  if (self == NULL) {
    return;
  }

  taskENTER_CRITICAL();
  self->rx_tail = self->rx_head;
  taskEXIT_CRITICAL();
}

/**
 * @brief 处理远程控制数据字节流
 * @param self 裁判系统接收对象
 * @param chunk 待处理的数据块指针
 * @param chunk_len 数据块长度
 *
 * 此函数负责接收并解析远程控制数据，包括粘包处理、帧同步和数据校验
 */
static void Game_FeedBytes(Game_t *self, const uint8_t *chunk,
                           size_t chunk_len)
{
  uint8_t work_buf[GAME_STASH_CAPACITY + GAME_RX_BATCH_LEN]; // 工作缓冲区
  size_t total = 0U;                                        // 总数据长度
  size_t pos = 0U;                                          // 当前处理位置

  if ((self == NULL) || (chunk == NULL) || (chunk_len == 0U) ||
      (self->target == NULL)) {
    return; // 参数检查：如果数据块为空、长度为0或目标对象为空，则直接返回
  }

  if (self->stash_len > 0U) {
    // 如果存在之前未处理完的数据（粘包），先复制到工作缓冲区
    memcpy(work_buf, self->stash, self->stash_len);
    total = self->stash_len;
  }

  if (chunk_len > (sizeof(work_buf) - total)) {
    // 防止缓冲区溢出，限制本次处理的数据长度
    chunk_len = sizeof(work_buf) - total;
  }

  memcpy(work_buf + total, chunk, chunk_len); // 将新数据追加到工作缓冲区
  total += chunk_len;

  while (pos < total) {
    size_t remain = total - pos; // 剩余待处理数据长度

    if (work_buf[pos] != JUDGE_System_FH) {
      // 寻找帧头标识符，如果不是帧头则继续搜索
      pos++;
      continue;
    }

    if (remain < GAME_MIN_FRAME_LEN) {
      // 剩余数据不足一帧最小长度，跳出循环
      break;
    }

    size_t frame_len = Game_GetFrameLen(&work_buf[pos]); // 获取帧长度

    if ((frame_len < GAME_MIN_FRAME_LEN) ||
        (frame_len > GAME_MAX_FRAME_LEN)) {
      // 帧长度不在有效范围内，跳过当前字节继续搜索
      pos++;
      continue;
    }

    if (remain < frame_len) {
      // 剩余数据不足以构成完整帧，跳出循环
      break;
    }

    remote_process(&work_buf[pos], self->target); // 处理完整的数据帧
    pos += frame_len;                             // 移动到下一帧的起始位置
  }

  self->stash_len = total - pos; // 计算剩余未处理的数据长度
  if (self->stash_len > sizeof(self->stash)) {
    // 如果剩余数据超过缓存容量，调整起始位置以截取最新的数据
    pos = total - sizeof(self->stash);
    self->stash_len = sizeof(self->stash);
  }

  if (self->stash_len > 0U) {
    // 将未处理的数据保存到stash缓冲区，用于下次处理
    memcpy(self->stash, &work_buf[pos], self->stash_len);
  }
}

//! RX DMA 接收回调（中断上下文）：写入 FIFO 后通知任务解析。
static void Game_RxCallback(uint8_t *data, size_t size)
{
  if ((data == NULL) || (size == 0U) || (instance_ == NULL)) {
    return;
  }

  for (size_t i = 0U; i < size; ++i) {
    Game_PushRxByteFromISR(instance_, data[i]);
  }

  if (instance_->thread_alert != NULL) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(instance_->thread_alert, GAME_SIGNAL_RAW_READY, eSetBits,
                       &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/**
 * @brief 初始化遥控器数据
 *
 * 此函数负责初始化自定义机器人数据结构，将其所有字段清零。
 */
void remote_control_data_init(void)
{
  // 清空自定义机器人数据结构的所有内容
  memset(&custom_robot_data, 0, sizeof(custom_robot_data));
}

/**
 * @brief 处理遥控器数据
 *
 * 解析传入的数据帧，验证帧头、长度和CRC校验，
 * 然后根据命令ID将数据复制到相应的结构体字段中。
 *
 * @param data 指向接收到的数据帧的指针
 * @param custom_robot_data 指向通信数据结构体的指针
 */
void remote_process(uint8_t *data, CommuniCateTypeDef *custom_robot_data)
{
  // 检查输入参数是否有效
  if ((data == NULL) || (custom_robot_data == NULL)) {
    return;
  }

  // 检查帧头是否匹配预期值
  if (data[SOF] != JUDGE_System_FH) {
    return;
  }

  // 从数据帧中提取数据长度信息
  uint16_t data_len =
      (uint16_t)(data[DATA_LENGTH] | ((uint16_t)data[DATA_LENGTH + 1U] << 8U));
  // 计算整个帧的长度
  size_t frame_len = Game_GetFrameLen(data);

  // 验证帧长度是否在允许的范围内
  if ((frame_len < GAME_MIN_FRAME_LEN) || (frame_len > GAME_MAX_FRAME_LEN)) {
    return;
  }

  // 验证帧头的CRC8校验
  if (Verify_CRC8_Check_Sum(data, Frame_header_LEN) == 0U) {
    return;
  }
  // 验证整个帧的CRC16校验
  if (Verify_CRC16_Check_Sum(data, (uint32_t)frame_len) == 0U) {
    return;
  }

  // 提取命令ID
  uint16_t com =
      (uint16_t)(data[RM_CMD_ID] | ((uint16_t)data[RM_CMD_ID + 1U] << 8U));
  // 指向有效载荷数据的指针
  uint8_t *payload = data + RM_DATA;
  // 指向裁判系统数据的指针
  data_t *judge_data = &custom_robot_data->judge_data;

  /**
   * @brief 宏定义：将有效载荷数据复制到指定字段
   *
   * 如果数据长度足够，则将有效载荷数据复制到目标字段。
   *
   * @param field 要复制数据的目标字段
   */
#define COPY_PAYLOAD(field)                                                    \
  do {                                                                         \
    if (data_len >= (uint16_t)sizeof(field)) {                                 \
      memcpy(&(field), payload, sizeof(field));                                \
    }                                                                          \
  } while (0)

  // 根据命令ID处理不同类型的数据
  switch (com) {
  case GAME_STATE_ID:
    COPY_PAYLOAD(judge_data->game_status); // 游戏状态数据
    break;
  case GAME_RESULT_ID:
    COPY_PAYLOAD(judge_data->game_result); // 游戏结果数据
    break;
  case GAME_ROBOT_ID:
    COPY_PAYLOAD(judge_data->game_robot_hp); // 游戏机器人血量数据
    break;
  case EVENT_DATA_ID:
    COPY_PAYLOAD(judge_data->event_data); // 事件数据
    break;
  case JUDGE_WARN_ID:
    COPY_PAYLOAD(judge_data->referee_warning); // 裁判警告数据
    break;
  case DART_INFO_ID:
    COPY_PAYLOAD(judge_data->dart_info); // 飞镖信息数据
    break;
  case ROBOT_STATUS_ID:
    COPY_PAYLOAD(judge_data->robot_status); // 机器人状态数据
    break;
  case POWER_HEAR_ID:
    COPY_PAYLOAD(judge_data->power_heat_data); // 功率热量数据
    break;
  case ROBOT_POS_ID:
    COPY_PAYLOAD(judge_data->robot_pos); // 机器人位置数据
    break;
  case BUFF_ID:
    COPY_PAYLOAD(judge_data->buff); // 增益数据
    break;
  case HURT_ID:
    COPY_PAYLOAD(judge_data->hurt_data); // 伤害数据
    break;
  case SHOOT_ID:
    COPY_PAYLOAD(judge_data->shoot_data); // 射击数据
    break;
  case SHOOT_ALLOW_ID:
    COPY_PAYLOAD(judge_data->projectile_allowance); // 弹丸许可数据
    break;
  case RFID_STATUS_ID:
    COPY_PAYLOAD(judge_data->rfid_status); // RFID状态
    break;
  case DART_CLIENT_CMD_ID:
    COPY_PAYLOAD(judge_data->dart_client_cmd); // 飞镖客户端命令数据
    break;
  case GROUND_ROBOT_POS_ID:
    COPY_PAYLOAD(judge_data->ground_robot_position); // 地面机器人位置数据
    break;
  case RADAR_MARK_DATA_ID:
    COPY_PAYLOAD(judge_data->radar_mark_data); // 雷达标记数据
    break;
  case SENTRY_INFO_ID:
    COPY_PAYLOAD(judge_data->sentry_info); // 哨兵信息数据
    break;
  case RADAR_INFO_ID:
    COPY_PAYLOAD(judge_data->radar_info); // 雷达信息数据
    break;
  case ROBOT_INTERACTION_ID:
    COPY_PAYLOAD(custom_robot_data->robot_interaction_data); // 机器人交互数据
    break;
  case CUSTOM_CONTROLLER_ID:
    COPY_PAYLOAD(custom_robot_data->custom_robot_data); // 自定义控制器数据
    break;
  case map_find:
    COPY_PAYLOAD(custom_robot_data->map_command); // 地图命令数据
    break;
  case remote_control_ID:
    COPY_PAYLOAD(custom_robot_data->remote_control_data); // 遥控器数据
    break;
  default:
    break;
  }

  // 取消定义COPY_PAYLOAD宏
#undef COPY_PAYLOAD
}

//! 初始化 game 对象，绑定 UART RX DMA 和解析目标。
err_t Game_Init(Game_t *self, UART_HandleTypeDef *uart_handle,
                CommuniCateTypeDef *target)
{
  if (self == NULL) {
    return PTR_NULL;
  }

  memset(self, 0, sizeof(*self));
  self->target = target;
  self->init_error_ = STM32UART_Init(
      &self->uart_receive_, uart_handle,
      (BSP_UART_RawData_t){game_rx_dma_buff_, sizeof(game_rx_dma_buff_)},
      Game_RxCallback);
  instance_ = self;

  if (target == NULL) {
    self->init_error_ = PTR_NULL;
  }

  return self->init_error_;
}

//! 启动 UART 循环 DMA 接收。
err_t Game_Start(Game_t *self)
{
  if (self == NULL) {
    return PTR_NULL;
  }
  if (self->init_error_ != OK) {
    return self->init_error_;
  }

  self->rx_head = 0U;
  self->rx_tail = 0U;
  self->rx_fifo_overflowed = false;
  self->rx_fifo_overflow_count = 0U;
  Game_ResetStash(self);

  return STM32UART_SetRxDMA(&self->uart_receive_);
}

//! 任务周期入口：等待 RX 通知，处理 FIFO 和裁判系统帧解析。
void Game_Update(Game_t *self, uint32_t timeout_ms)
{
  if (self == NULL) {
    return;
  }

  xTaskNotifyWait(0U, GAME_SIGNAL_RAW_READY, NULL, pdMS_TO_TICKS(timeout_ms));

  if (Game_ConsumeRxOverflow(self)) {
    Game_ClearRxFifo(self);
    Game_ResetStash(self);
  }

  uint8_t local_buf[GAME_RX_BATCH_LEN];
  uint16_t count = 0U;
  while (Game_PopRxBatch(self, local_buf, (uint16_t)sizeof(local_buf),
                         &count)) {
    Game_FeedBytes(self, local_buf, count);
  }

  if (Game_ConsumeRxOverflow(self)) {
    Game_ClearRxFifo(self);
    Game_ResetStash(self);
  }
}
