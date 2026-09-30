#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "bsp_uart.h"
#include "comp_cmd.h"
#include "comp_def.h"
#include "task.h"

// ==================== VOFA 协议与缓冲常量 ====================

#define VOFA_TAIL_LEN (4u)       //! firewater 帧尾长度：00 00 80 7F
#define VOFA_MAX_FLOATS (6u)     //! 单帧最多发送的 float 数量
#define VOFA_TX_LEN (VOFA_MAX_FLOATS * sizeof(float) + VOFA_TAIL_LEN)  //! TX 帧最大长度
#define VOFA_RX_LEN (128u)       //! ISR 到任务的环形 FIFO 长度，必须为 2 的幂
#define VOFA_RX_DMA_BUF_LEN (64u)       //! RX DMA 原始接收缓冲长度
#define VOFA_RX_CMD_BUF_LEN (64u)       //! ASCII 命令累积缓冲长度

#ifdef __cplusplus
extern "C" {
#endif

//! 命令回调：解析到合法 name=value! 命令后调用。
typedef void (*Vofa_CommandCallback_t)(const char *name, float value);

//! VOFA 对象。
//! TX 发送 firewater 浮点帧，RX 接收 ASCII 命令并在任务上下文解析。
typedef struct {
  TaskHandle_t thread_alert;                    //! 接收事件通知的 FreeRTOS 任务句柄
  STM32UARTFrameTx_t uart_send_;                //! VOFA 变长帧发送通道
  STM32UART_t uart_receive_;                    //! VOFA 命令接收通道
  err_t tx_init_error_;                         //! TX BSP 初始化结果，Start 前保留
  err_t rx_init_error_;                         //! RX BSP 初始化结果，Start 前保留
  Vofa_CommandCallback_t command_callback_;     //! 命令解析完成后的业务回调
} Vofa_t;

//! 初始化 VOFA 对象，绑定 UART 句柄和收发 DMA 缓冲。
err_t Vofa_Init(Vofa_t *self, UART_HandleTypeDef *uart_handle);
//! 启动 TX/RX DMA。
err_t Vofa_Start(Vofa_t *self);
//! 任务周期入口：处理 RX FIFO、解析 ASCII 命令和溢出恢复。
void Vofa_Update(Vofa_t *self, uint32_t timeout_ms);
//! 设置命令回调，不会影响正在进行的 DMA 收发。
void Vofa_SetCommandCallback(Vofa_t *self, Vofa_CommandCallback_t callback);
//! 按名称读取参数表；参数存在且至少收到过一次时返回 true。
bool Vofa_GetParameter(const char *name, float *value);
//! 读取 speed 参数。
bool Vofa_GetSpeed(float *speed);
//! 读取 angle 参数。
bool Vofa_GetAngle(float *angle);
//! 按 VOFA firewater 格式发送 float 数组：payload 后追加 00 00 80 7F 帧尾。
//! 返回 BUSY 表示本帧未入队，可以在后续采样周期重试。
err_t Vofa_Send(Vofa_t *self, const float *data, size_t size);

#ifdef __cplusplus
}
#endif
