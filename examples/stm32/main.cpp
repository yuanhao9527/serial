/*
 * 串口 HAL 抽象层 —— STM32 应用示例（板级 glue + 应用逻辑）。
 *
 * 使用 ports/stm32 端口（Stm32Uart）。应用层只持有 ISerial*，不依赖
 * UART_HandleTypeDef / HAL_xxx；与 examples/host 的应用逻辑完全一致，
 * 仅板级实例化那一行不同。
 *
 * 集成要点：
 *   - 工程已由 CubeMX 生成 HAL 初始化（UART + DMA），本示例 board.* 仅为参考。
 *   - 编译加入：include/ 、 src/serial_core.cpp 、 ports/stm32/ 、 examples/stm32/
 *     并配好芯片宏（如 STM32F103xE、USE_HAL_DRIVER）。
 *   - 切勿在 usart.c 中重复实现 HAL_UARTEx_RxEventCallback /
 *     HAL_UART_TxCpltCallback / HAL_UART_ErrorCallback —— 它们已由
 *     ports/stm32/stm32_uart.cpp 内部接管。
 */
#include "stm32_uart.h"
#include "iserial.h"
#include "board.h"
#include <cstdio>

ISerial* g_serial = nullptr;
Stm32Uart u1(&huart1);          /* 唯一的板级具体类型，其余全用 ISerial */

/* 应用层回调：只认 ISerial&，不碰 UART_HandleTypeDef / HAL */
void onRx(ISerial& ser, uint8_t* data, uint16_t len) {
    ser.send(data, len);        /* 示例直接回显 */
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    u1.init();                  /* 启动接收（内部启动 ReceiveToIdle_DMA） */
    u1.setRxCallback(onRx);
    g_serial = &u1;

    g_serial->send((const uint8_t*)"serial-hal ready\r\n", 19);

    while (1) {
        /* 应用主循环，串口收发全在中断 / 回调中异步完成 */
    }
}

/* printf 重定向（可选） */
extern "C" int fputc(int ch, FILE* f) {
    if (g_serial) g_serial->send((const uint8_t*)&ch, 1);
    return ch;
}
