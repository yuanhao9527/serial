# 示例（examples）

示例演示「应用层只持有 `ISerial*`、注册 `onRx` 回调」的使用方式，与具体
端口类型解耦。要接新平台，只需在 `ports/<平台>/` 实现 4 个钩子，应用代码不变。

| 目录 | 端口 | 用途 | 可编译运行环境 |
|------|------|------|----------------|
| `ports/host/` | `HostUart`（无硬件，stdout 模拟） | 在 PC 上验证核心逻辑、做单元测试 | 任意 C++ 编译器（g++/clang） |
| `examples/host/main.cpp` | —（应用示例） | 只用 `ISerial*`，演示收发 / 回显 | 配合 `ports/host/` 在 PC 运行 |
| `examples/stm32/` | `Stm32Uart`（`ports/stm32`） | 真实 MCU 上的板级集成参考 | STM32Cube HAL 工程 |

## host（PC 可运行，使用 ports/host 端口）

```
g++ -Iinclude -Isrc -Iports/host \
    src/serial_core.cpp ports/host/host_uart.cpp examples/host/main.cpp \
    -o serial_demo
./serial_demo
```

输出两次 `hello serial-hal` 与 `ping`。`simulateRx()` 用于模拟「外部来帧」，
触发 RX 回调，无需真实串口。

## stm32（MCU，使用 ports/stm32 端口）

见 `examples/stm32/README.md`：CubeMX 的 DMA 配置、`HAL_xxx` 回调由库接管、
编译目录与芯片宏等集成步骤。应用逻辑与 host 示例一致，仅
`Stm32Uart u1(&huart1);` 一行不同。
