#ifndef STM32_UART_H
#define STM32_UART_H

#include "stm32f1xx_hal.h"
#include "iserial.h"

/* RX 缓冲：DMA 循环写入，处理不过来就调大（128/256...） */
#define UART_RX_SIZE    64
/* TX 环形缓冲大小：发送量大就调大 */
#define UART_TX_SIZE    128
/* 最多支持的串口实例数 */
#define UART_MAX        3

/**
 * @brief  STM32 HAL 串口实现（继承 ISerial）。
 *         每个实例自带 RX/TX 缓冲，互不干扰。
 * @note   用法：
 *           Stm32Uart u1(&huart1);        // 绑定 CubeMX 生成的句柄
 *           u1.init();                     // 启动 DMA 循环接收 + 空闲线
 *           u1.send(buf, len);            // 非阻塞发送
 *           u1.setRxCallback(onRx);        // 注册应用层解析（只认 ISerial&）
 */
class Stm32Uart : public ISerial {
public:
    explicit Stm32Uart(UART_HandleTypeDef* huart);

    void init(void) override;                              /* 启动 RX: DMA 循环 + 空闲线 */
    int  send(const uint8_t* data, uint16_t len) override; /* 非阻塞发送（TX 环形缓冲） */

    /* 以下三个供 HAL 全局回调调用，应用层勿直接调 */
    void onTxComplete(void);                              /* HAL_UART_TxCpltCallback */
    void onError(void);                                   /* HAL_UART_ErrorCallback */
    void dispatchRx(uint16_t size);                       /* HAL_UARTEx_RxEventCallback */

    /* 按外设实例反查类实例（供全局回调使用） */
    static Stm32Uart* find(UART_HandleTypeDef* huart);

private:
    UART_HandleTypeDef* huart_;
    uint8_t  rx_buf_[UART_RX_SIZE];
    uint8_t  tx_buf_[UART_TX_SIZE];
    volatile uint16_t tx_head_;
    volatile uint16_t tx_tail_;
    volatile uint8_t  tx_busy_;
    volatile uint16_t tx_dma_len_;

    void txFeed(void);                                    /* 从 TX 环形缓冲取一段启动 DMA */
    void restartRx(void);                                 /* 重启 RX（错误恢复） */
    void reg(void);                                       /* 注册进实例表 */

    static Stm32Uart* registry_[UART_MAX];
    static int   regCount_;
};

#endif /* STM32_UART_H */
