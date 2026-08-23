#include "stm32_uart.h"
#include <string.h>

/* 静态成员定义 */
Stm32Uart* Stm32Uart::registry_[UART_MAX] = {nullptr};
int   Stm32Uart::regCount_ = 0;

Stm32Uart::Stm32Uart(UART_HandleTypeDef* huart)
    : huart_(huart), tx_head_(0), tx_tail_(0), tx_busy_(0), tx_dma_len_(0)
{
    reg();
}

/* 注册自身到静态表，供全局回调按实例反查 */
void Stm32Uart::reg(void) {
    if (regCount_ < UART_MAX) {
        registry_[regCount_++] = this;
    }
}

/* 按外设句柄反查类实例 */
Stm32Uart* Stm32Uart::find(UART_HandleTypeDef* huart) {
    for (int i = 0; i < regCount_; i++) {
        if (registry_[i] && registry_[i]->huart_ == huart) return registry_[i];
    }
    return nullptr;
}

void Stm32Uart::init(void) {
    /* 启动 DMA 循环接收 + 空闲线检测 */
    HAL_UARTEx_ReceiveToIdle_DMA(huart_, rx_buf_, UART_RX_SIZE);
    __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);
}

/* 从 TX 环形缓冲取一段启动 DMA 发送 */
void Stm32Uart::txFeed(void) {
    if (tx_busy_) return;

    uint16_t head = tx_head_;
    uint16_t tail = tx_tail_;
    if (head == tail) return;            /* 空，没东西可发 */

    uint16_t len = (head >= tail) ? (head - tail) : (UART_TX_SIZE - tail);

    tx_dma_len_ = len;
    tx_busy_ = 1;
    HAL_UART_Transmit_DMA(huart_, (uint8_t*)&tx_buf_[tail], len);
}

int Stm32Uart::send(const uint8_t* data, uint16_t len) {
    uint16_t head = tx_head_;
    uint16_t tail = tx_tail_;

    /* 计算剩余空间 */
    uint16_t free = (tail > head) ? (tail - head - 1)
                                  : (UART_TX_SIZE - head + tail - 1);
    if (len > free) {
        len = free;                      /* 空间不够：最多发 free 字节，其余丢弃 */
    }
    if (len == 0) return 0;

    __disable_irq();
    for (uint16_t i = 0; i < len; i++) {
        tx_buf_[tx_head_] = data[i];
        tx_head_ = (tx_head_ + 1) % UART_TX_SIZE;
    }
    __enable_irq();

    txFeed();
    return (int)len;
}

/* DMA 发送完成：推进 tail，继续发环形缓冲里剩下的 */
void Stm32Uart::onTxComplete(void) {
    tx_tail_ = (tx_tail_ + tx_dma_len_) % UART_TX_SIZE;
    tx_busy_ = 0;
    txFeed();
}

void Stm32Uart::onError(void) {
    restartRx();
}

void Stm32Uart::restartRx(void) {
    __HAL_UART_DISABLE_IT(huart_, UART_IT_IDLE);
    HAL_UART_Abort(huart_);
    HAL_UARTEx_ReceiveToIdle_DMA(huart_, rx_buf_, UART_RX_SIZE);
    __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);
}

/* 收到一帧（空闲线）：交给 ISerial::onData（默认转发回调 / 回发） */
void Stm32Uart::dispatchRx(uint16_t size) {
    if (size > 0 && size <= UART_RX_SIZE) {
        onData(rx_buf_, size);
    }
}

/* ===================== HAL 全局回调 ===================== */
extern "C" {

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
    Stm32Uart* u = Stm32Uart::find(huart);
    if (u) u->dispatchRx(Size);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
    Stm32Uart* u = Stm32Uart::find(huart);
    if (u) u->onTxComplete();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    Stm32Uart* u = Stm32Uart::find(huart);
    if (u) u->onError();
}

}
