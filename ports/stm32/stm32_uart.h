#ifndef STM32_UART_H
#define STM32_UART_H

#include "serial_core.h"

/* HAL 芯片头可由工程宏覆盖，例如 -DSTM32_HAL_HEADER="\"stm32f4xx_hal.h\"" */
#ifndef STM32_HAL_HEADER
#define STM32_HAL_HEADER "stm32f1xx_hal.h"
#endif
#include STM32_HAL_HEADER

#ifndef STM32_UART_RX_SIZE
#define STM32_UART_RX_SIZE   64
#endif
#ifndef STM32_UART_TX_SIZE
#define STM32_UART_TX_SIZE   128
#endif
#ifndef STM32_UART_MAX
#define STM32_UART_MAX       3
#endif
/* 溢出防护保留区：HT/TC 保证回调间隔 ≤ 半缓冲，正常流量到不了这里；
 * 若写位置逼近未读区超过此值，说明回调被饿死、数据已被覆盖 */
#ifndef STM32_UART_RX_RESERVE
#define STM32_UART_RX_RESERVE (STM32_UART_RX_SIZE / 4)
#endif

/**
 * @brief  STM32 HAL 后端：BufferedSerial 的一个具体实现。
 *         整个库里只有这一层出现 UART_HandleTypeDef / HAL_xxx。
 *         接其他 MCU 时，照此再写一个 ports/<平台>/xxx_uart.{h,cpp} 即可。
 */
class Stm32Uart : public BufferedSerial {
public:
    explicit Stm32Uart(UART_HandleTypeDef* huart);

    /* HAL 全局回调入口（公开以便 extern "C" 回调调用）：
     * pos 为 DMA 写位置（已收字节数），内部处理循环回卷后整段上抛；
     * 帧边界由 huart->RxEventType 区分（旧版 HAL 一律按完整帧上报） */
    void rxEvent(uint16_t pos);

    /* 溢出防护触发次数（回调饿死导致数据被覆盖后重启接收的次数） */
    uint32_t rxOverruns(void) const { return rxOverruns_; }

    /* 供全局 HAL 回调按外设反查实例 */
    static Stm32Uart* find(UART_HandleTypeDef* huart);

protected:
    bool startTransmit(const uint8_t* data, uint16_t len) override;
    void startReceive(void) override;
    void restartReceive(void) override;
    void lockCritical(void) override;
    void unlockCritical(void) override;

private:
    UART_HandleTypeDef* huart_;
    uint8_t  rxBuf_[STM32_UART_RX_SIZE];
    uint8_t  txBuf_[STM32_UART_TX_SIZE];
    /* 环形 DMA 回卷整理：跨边界帧拷贝成线性后一次上抛 */
    uint8_t  rxLinear_[STM32_UART_RX_SIZE];
    uint16_t rxReadIdx_ = 0;
    uint32_t rxOverruns_ = 0;
    /* 临界区：深度计数支持同实例嵌套；仅最外层保存/恢复 PRIMASK，
     * 在中断里调用也安全（恢复而非无条件 __enable_irq） */
    uint32_t primaskSave_ = 0;
    uint16_t critDepth_   = 0;

    void reg(void);
    static Stm32Uart* registry_[STM32_UART_MAX];
    static int   regCount_;
};

#endif /* STM32_UART_H */
