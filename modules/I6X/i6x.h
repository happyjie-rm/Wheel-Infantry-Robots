#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "bsp_uart.h"
#include "comp_cmd.h"
#include "comp_def.h"
#include "task.h"

// ==================== I6x / iBus 协议常量 ====================

#define I6X_FRAME_SIZE (32u)          //! iBus 一帧固定 32 字节。
#define I6X_FRAME_LENGTH (0x20u)      //! iBus 帧长度字段。
#define I6X_FRAME_COMMAND (0x40u)     //! iBus 通道数据命令字。
#define I6X_CHANNEL_COUNT (14u)       //! iBus 标准最多 14 路通道。
#define I6X_USER_CHANNEL_COUNT (10u)  //! 当前 I6x 常用通道数量。
#define I6X_AUX_CHANNEL_COUNT (10u)   //! CH5-CH14 辅助通道数量。

#define I6X_OFFLINE_TIMEOUT_MS (20u)      //! 超过该时间未收到合法帧则认为离线。
#define I6X_CH_VALUE_MIN (1000u)          //! iBus 常见通道最小值。
#define I6X_CH_VALUE_MID (1500u)          //! iBus 常见通道中值。
#define I6X_CH_VALUE_MAX (2000u)          //! iBus 常见通道最大值。

#ifdef __cplusplus
extern "C" {
#endif

//! 字节流帧同步状态。bsp_uart 回调可能给出任意长度片段，需逐字节拼帧。
typedef struct {
  uint8_t buffer[I6X_FRAME_SIZE];
  size_t index;
} i6x_frame_sync_t;

//! I6x 解析后的遥控器语义。
//! CH1-CH4 按常见航模映射为右摇 X/Y、左摇 Y/X；CH5-CH14 保留为辅助通道。
typedef struct {
  struct {
    vector2_t l;
    vector2_t r;
  } ch;

  uint16_t channel[I6X_CHANNEL_COUNT];  //! 14 路原始通道值。
  float aux[I6X_AUX_CHANNEL_COUNT];     //! CH5-CH14 归一化辅助通道。
  uint16_t checksum_cal;                //! 本地计算出的校验值。
  uint16_t checksum_rx;                 //! 帧内携带的校验值。

  struct {
    uint8_t length;
    uint8_t command;
    bool valid;
  } frame;
} i6x_cmd_rc_t;

//! I6x 遥控器对象。
//! RX 使用 UART DMA 接收字节流，任务上下文解析为 i6x_cmd_rc_t。
typedef struct {
  TaskHandle_t thread_alert;   //! 接收事件通知的 FreeRTOS 任务句柄。
  BaseType_t switch_required;  //! 预留状态切换标志。
  i6x_cmd_rc_t cmd;            //! 最近一次合法解析结果。
  bool online_;                //! 最近周期是否收到并解析到合法帧。
  STM32UART_t uart_;           //! UART DMA 接收通道。
  err_t init_error_;           //! BSP 初始化结果，Start 前保留。
  uint8_t raw_frame[I6X_FRAME_SIZE];  //! 最近一次完整原始帧快照。
} I6X_t;

//! 复位 iBus 字节流帧同步状态。
void I6X_FrameSyncReset(i6x_frame_sync_t *sync);
//! 向帧同步器输入 1 字节；返回 true 时 out_frame 已得到 32 字节候选帧。
bool I6X_FrameSyncPush(i6x_frame_sync_t *sync, uint8_t byte,
                       uint8_t out_frame[I6X_FRAME_SIZE]);
//! 解码并校验 32 字节 iBus 原始帧。
err_t I6X_DecodeFrame(const uint8_t frame[I6X_FRAME_SIZE], i6x_cmd_rc_t *cmd);

//! 初始化 I6x 对象，绑定 UART 句柄和 RX DMA 缓冲。
err_t I6X_Init(I6X_t *self, UART_HandleTypeDef *uart_handle);
//! 启动 I6x RX DMA。
err_t I6X_Start(I6X_t *self);
//! 任务周期入口：等待完整帧通知，解析成功则置 online_。
void I6X_Update(I6X_t *self, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

