#include "host_uart.h"
#include <cstring>
#include <cstdio>

HostUart::HostUart()
    : BufferedSerial(rxBuf_, sizeof(rxBuf_), txBuf_, sizeof(txBuf_))
{
}

void HostUart::simulateRx(const uint8_t* data, uint16_t len) {
    if (len > 0 && len <= sizeof(rxScratch_)) {
        memcpy(rxScratch_, data, len);
        onRxData(rxScratch_, len);
    }
}

bool HostUart::startTransmit(const uint8_t* data, uint16_t len) {
    /* 在此「发出」：示例直接打印到 stdout，并立即视为发送完成 */
    fwrite(data, 1, len, stdout);
    fflush(stdout);
    onTxComplete();
    return true;
}

void HostUart::startReceive(void) {
    /* 主机示例无需真正启动硬件接收；外部通过 injectRx 模拟 */
}

void HostUart::lockCritical(void) { /* 单线程示例无需关中断 */ }
void HostUart::unlockCritical(void)  { /* 单线程示例无需开中断 */ }
