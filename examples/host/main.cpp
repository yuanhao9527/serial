/*
 * 串口 HAL 抽象层 —— 应用示例（主机可运行版）。
 *
 * 编译（需 C++ 编译器，如 g++）：
 *   g++ -Iinclude -Isrc -Iexamples/host \
 *       src/serial_core.cpp ports/host/host_uart.cpp examples/host/main.cpp \
 *       -o serial_demo
 * 运行：
 *   ./serial_demo
 *
 * 应用层只持有 ISerial*，通过 ISerial& 交互，不依赖任何具体端口类型。
 */
#include "host_uart.h"
#include "iserial.h"
#include <cstdio>
#include <cstring>

ISerial* g_serial = nullptr;
HostUart u1;                /* 唯一的板级具体类型，其余全用 ISerial */

/* 应用层回调：只认 ISerial&，不碰任何具体硬件 / 端口 */
void onRx(ISerial& ser, uint8_t* data, uint16_t len) {
    printf("app received %u bytes: ", (unsigned)len);
    fwrite(data, 1, len, stdout);
    printf("\n");
    ser.send(data, len);   /* 回显 */
}

int main(void) {
    u1.init();
    u1.setRxCallback(onRx);
    g_serial = &u1;

    /* 模拟外部设备发来一帧：经核心分发到 onRx */
    const char* msg = "hello serial-hal";
    u1.simulateRx((const uint8_t*)msg, (uint16_t)strlen(msg));

    /* 应用层直接发送：走 TX 环形缓冲 + pumpTx */
    g_serial->send((const uint8_t*)"ping\r\n", 7);
    return 0;
}
