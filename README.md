# stm32-serial

C++ 串口抽象库。**核心与硬件无关**，具体 MCU 通过「端口（port）」接入。

- 核心：`include/iserial.h` + `include/serial_core.h` / `src/serial_core.cpp`
  —— 只依赖 `<cstdint>`，不出现任何 MCU / HAL / 寄存器。
- 端口：`ports/stm32/` —— 唯一的硬件相关代码（STM32 HAL 后端）。

应用 / FOC 逻辑只持有 `ISerial*`，通过接收回调拿 `ISerial&`，与具体平台解耦。

## 目录结构
```
include/
  iserial.h        # 平台无关接口 ISerial
  serial_core.h    # 硬件无关核心 BufferedSerial（TX 环形缓冲 + RX 分发）
src/
  serial_core.cpp  # BufferedSerial 实现（无硬件）
ports/stm32/
  stm32_uart.h     # STM32 HAL 后端（仅此层有 UART_HandleTypeDef / HAL_xxx）
  stm32_uart.cpp
```

## 核心做了什么（全硬件无关）
- 接收：由硬件层在“收到一帧”时调用 `onRxData()`，核心再转给 `ISerial::onData`
  （默认转发给应用回调，未设回调则原样回发）。
- 发送：`send()` 非阻塞，写入 TX 环形缓冲，再由 `pumpTx()` 驱动硬件发送。
- 三个硬件钩子由子类实现：`writeHardware` / `startReceiveHardware` / `enterCritical`+`exitCritical`。

## 用法（应用层只碰接口）
```cpp
#include "stm32_uart.h"   // 换成你用的端口头文件
#include "iserial.h"

ISerial* g_serial = nullptr;
Stm32Uart u1(&huart1);    // 唯一出现具体类名（板级 glue）

void onRx(ISerial& ser, uint8_t* data, uint16_t len) {
    ser.send(data, len);  // 应用层只认 ISerial&
}

u1.init();                // 启动接收
u1.setRxCallback(onRx);   // 注册解析
g_serial = &u1;           // 之后应用层只用 g_serial

g_serial->send((uint8_t*)"hi\r\n", 4);

// printf 重定向（可选）
extern "C" int fputc(int ch, FILE* f) {
    if (g_serial) g_serial->send((uint8_t*)&ch, 1);
    return ch;
}
```

## 接一个新平台（例如某个非 STM32 的 MCU）
1. 新建 `ports/<平台>/xxx_uart.{h,cpp}`。
2. 让 `class XxxUart : public BufferedSerial`，实现 4 个虚函数：
   - `writeHardware(data,len)` —— 启动一次异步发送；
   - `startReceiveHardware()` —— 启动接收，收到一帧后调用 `onRxData(buf,len)`；
   - `enterCritical()` / `exitCritical()` —— 保护环形缓冲的临界区。
3. 在硬件的“发送完成 / 收到一帧 / 出错”中断里分别调用
   `onTxComplete()` / `onRxData(...)` / `onError()`。
4. 应用层代码一行都不用改（仍只持有 `ISerial*`）。

## 集成要求（以 STM32 端口为例）
- STM32Cube HAL（F1 系列；其他系列适配 `ports/stm32` 即可）。
- C++ 工程（关闭 Keil 的 `Use MicroLIB`，因其不支持 C++）。
- 编译时加入：`include/`、`src/`、`ports/stm32/`，并配好 HAL 头文件路径与芯片宏
  （如 `STM32F103xE`、`USE_HAL_DRIVER`）。
- 在 `usart.c` 中把 RX DMA 设为 `DMA_CIRCULAR`，TX DMA 设为 `DMA_NORMAL`。
- `HAL_UARTEx_RxEventCallback` / `HAL_UART_TxCpltCallback` / `HAL_UART_ErrorCallback`
  已由本库内部接管，无需在用户文件里实现。
