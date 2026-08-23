# AGENTS.md

C++ 串口 HAL 抽象层（`serial-hal`）。给 AI 协助本仓库时遵循以下约定。

## 架构原则（最重要）

- **核心与硬件无关**：`include/serial_core.h` + `src/serial_core.cpp` 只依赖
  `<cstdint>`，不得出现任何 MCU / 寄存器 / 平台特定类型或底层 API。
- **硬件相关收敛到端口**：`ports/<平台>/` 是唯一允许出现
  具体硬件类型 / 底层发送接收 API 的地方。新增平台 = 新建 `ports/<平台>/`。
- 应用层只持有 `ISerial*`，通过 `ISerial&` 交互，不依赖具体实现类名。

## 文件职责

- `include/iserial.h` —— 平台无关接口 `ISerial`，含 `onData` 回调约定。
- `include/serial_core.h` / `src/serial_core.cpp` —— 硬件无关核心
  `BufferedSerial`（TX 环形缓冲 + RX 分发）。子类实现 4 个钩子：
  `startTransmit` / `startReceive` / `lockCritical` / `unlockCritical`。
- `ports/<平台>/xxx_uart.{h,cpp}` —— 具体 HAL 后端（唯一硬件相关代码）。

## 行为约定

- 接收：硬件层在「收到一帧」时调用 `onRxData()`；核心转发给
  `ISerial::onData`（默认转发给应用回调，未设回调则原样回发）。
- 发送：`send()` 非阻塞，写入 TX 环形缓冲，由 `pumpTx()` 驱动硬件发送。
- 中断入口：底层“发送完成 / 收到一帧 / 出错”回调由库内部接管，
  不要在用户文件里重复实现。

## 代码风格

- 语言：C++。
- 命名：接口类 `ISerial`，核心类 `BufferedSerial`，端口类 `XxxUart`。
- 头文件守卫 + 明确包含。核心文件不要引入 STL 重依赖。
- 提交前确保 `<cstdint>` 是唯一核心层依赖。

## 集成约束

- 加入编译的目录：`include/`、`src/`、`ports/<平台>/`，并配好底层头路径
  与芯片宏。
- 接收宜用 DMA 循环模式，发送用 DMA 单次模式（具体由端口决定）。
