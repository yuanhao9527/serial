#ifndef HOST_UART_H
#define HOST_UART_H

#include "serial_core.h"
#include <cstdint>

/**
 * @brief  示例用「主机（PC）端口」：实现串口 HAL 抽象层的 4 个钩子，
 *         不依赖任何 MCU / 硬件。可作为「接一个新平台」的参考，
 *         也可在 PC 上直接编译运行，验证核心逻辑。
 *         真实项目应将其替换为对应平台的端口实现。
 */
class HostUart : public BufferedSerial {
public:
    HostUart();

    /* 模拟「外部收到一帧」：供 demo 的 main 调用，触发 RX 回调 */
    void simulateRx(const uint8_t* data, uint16_t len);

protected:
    bool startTransmit(const uint8_t* data, uint16_t len) override;
    void startReceive(void) override;
    void lockCritical(void) override;
    void unlockCritical(void) override;

private:
    uint8_t rxBuf_[64];
    uint8_t txBuf_[128];
    uint8_t rxScratch_[64];   /* 给 injectRx 拷贝用，避免触碰核心私有缓冲 */
};

#endif /* HOST_UART_H */
