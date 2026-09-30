# Vofa
VOFA firewater 上行浮点帧与 ASCII 下行命令。

TX 使用 `STM32UARTFrameTx_*`：按实际浮点数量构帧，每次只发送一帧，不补零到最大容量。硬件 DBM 为定长连续流，因此此模块使用独立的变长帧 DMA 接口。

`Vofa_Send()` 返回 `OK` 表示已接受；发送页和等待页均占用时返回 `BUSY`，不覆盖旧帧。周期采样任务可在下个周期提交新样本。临界区由 BSP 内部维护，RX FIFO 和命令解析流程不变。
