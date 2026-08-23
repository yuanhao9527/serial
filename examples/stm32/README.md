# STM32 示例

演示如何在 STM32 上使用串口 HAL 抽象层（端口见 `ports/stm32/`）。应用层代码
与 `examples/host` 完全一致——只持有 `ISerial*`，不出现 `UART_HandleTypeDef` /
`HAL_xxx`。唯一的板级差异是 `Stm32Uart u1(&huart1);` 这一行具体类型。

## 工程配置（CubeMX）

1. 配置 USART1 为异步模式，开启 RX/TX DMA：
   - RX DMA：`DMA_CIRCULAR`
   - TX DMA：`DMA_NORMAL`
2. 生成代码（库已在 `ports/stm32/stm32_uart.cpp` 内接管底层回调，用户文件里
   不要再实现 `HAL_UARTEx_RxEventCallback` / `HAL_UART_TxCpltCallback` /
   `HAL_UART_ErrorCallback`）。
3. 在 IDE/CMake 中加入编译目录：
   - `include/`
   - `src/serial_core.cpp`
   - `ports/stm32/stm32_uart.cpp` + `stm32_uart.h`
   - 本示例 `main.cpp` + `board.cpp` + `board.h`
4. 定义芯片宏（如 `STM32F103xE`、`USE_HAL_DRIVER`）。
5. C++ 工程：关闭 Keil 的 `Use MicroLIB`（其不支持 C++）。

## 关键点

- 收到一帧（空闲线或缓冲满）自动触发 `onRx`；`send()` 非阻塞，写入 TX 环形
  缓冲后由 DMA 完成中断继续泵送。
- 若要新增串口，再 `Stm32Uart u2(&huart2);` 即可，最多 `STM32_UART_MAX`
  （默认 3）个实例。

## 运行

编译下载后，串口助手发送任意数据会原样回显；启动时打印 `serial-hal ready`。
