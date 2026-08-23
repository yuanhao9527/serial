#ifndef STM32_UART_H
#define STM32_UART_H

#include "serial_core.h"
#include "stm32f1xx_hal.h"

#ifndef STM32_UART_RX_SIZE
#define STM32_UART_RX_SIZE   64
#endif
#ifndef STM32_UART_TX_SIZE
#define STM32_UART_TX_SIZE   128
#endif
#ifndef STM32_UART_MAX
#define STM32_UART_MAX       3
#endif

/**
 * @brief  STM32 HAL 后端：BufferedSerial 的一个具体实现。
 *         整个库里只有这一层出现 UART_HandleTypeDef / HAL_xxx。
 *         接其他 MCU 时，照此再写一个 ports/<平台>/xxx_uart.{h,cpp} 即可。
 */
class Stm32Uart : public BufferedSerial {
public:
    explicit Stm32Uart(UART_HandleTypeDef* huart);

    /* HAL 全局回调入口（公开以便 extern "C" 回调调用） */
    void rxEvent(uint16_t size) { onRxData(rxBuf_, size); }

    /* 供全局 HAL 回调按外设反查实例 */
    static Stm32Uart* find(UART_HandleTypeDef* huart);

protected:
    bool writeHardware(const uint8_t* data, uint16_t len) override;
    void startReceiveHardware(void) override;
    void restartReceiveHardware(void) override;
    void enterCritical(void) override;
    void exitCritical(void) override;

private:
    UART_HandleTypeDef* huart_;
    uint8_t  rxBuf_[STM32_UART_RX_SIZE];
    uint8_t  txBuf_[STM32_UART_TX_SIZE];

    void reg(void);
    static Stm32Uart* registry_[STM32_UART_MAX];
    static int   regCount_;
};

#endif /* STM32_UART_H */
