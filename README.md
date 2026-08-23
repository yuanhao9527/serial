# stm32-serial

C++ 串口抽象库（STM32 HAL）。把应用 / FOC 逻辑与具体串口外设解耦。

## 组成
- `include/iserial.h` —— 平台无关接口 `ISerial`（`init` / `send` / `onData` / `setRxCallback`）。
- `include/stm32_uart.h` + `src/stm32_uart.cpp` —— STM32 HAL 实现 `Stm32Uart`，
  基于 DMA + 空闲线（Idle-Line）接收、TX 环形缓冲、非阻塞发送。
  每个实例自带 RX/TX 缓冲，支持多串口。

## 特性
- 接收：DMA 循环接收 + 空闲线触发，每帧（以空闲线为界）调用 `onData`。
- 发送：`send()` 非阻塞，写入 TX 环形缓冲后由 DMA 发出。
- 解耦：应用层只持有 `ISerial*`，通过接收回调拿到 `ISerial&`，不依赖 STM32。

## 用法
```cpp
#include "stm32_uart.h"
#include "iserial.h"

ISerial* g_serial = nullptr;
Stm32Uart u1(&huart1);          // 唯一出现具体类名（板级 glue）

// 应用层解析：只认 ISerial&
void onRx(ISerial& ser, uint8_t* data, uint16_t len) {
    ser.send(data, len);        // 示例：回显
}

// 初始化（CubeMX 生成外设之后）
u1.init();                      // 启动 DMA 循环接收 + 空闲线
u1.setRxCallback(onRx);         // 注册解析
g_serial = &u1;                 // 之后应用层只用 g_serial

// 发送（任意处）
g_serial->send((uint8_t*)"hi\r\n", 4);

// printf 重定向（可选）
extern "C" int fputc(int ch, FILE* f) {
    if (g_serial) g_serial->send((uint8_t*)&ch, 1);
    return ch;
}
```

## 集成要求
- STM32Cube HAL（F1 系列；其他系列需少量适配）。
- C++ 工程（关闭 Keil 的 `Use MicroLIB`，因其不支持 C++）。
- 编译时加入 `include/` 与 `src/`，并配置好 HAL 头文件搜索路径与芯片宏
  （如 `STM32F103xE`、`USE_HAL_DRIVER`）。
- 在 `usart.c` 中把 RX DMA 设为 `DMA_CIRCULAR`，TX DMA 设为 `DMA_NORMAL`。
- `HAL_UARTEx_RxEventCallback` / `HAL_UART_TxCpltCallback` / `HAL_UART_ErrorCallback`
  由本库内部接管，无需在用户文件里实现。
