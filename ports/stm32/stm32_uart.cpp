#include "stm32_uart.h"
#include <string.h>

Stm32Uart* Stm32Uart::registry_[STM32_UART_MAX] = {nullptr};
int   Stm32Uart::regCount_ = 0;

Stm32Uart::Stm32Uart(UART_HandleTypeDef* huart)
    : BufferedSerial(rxBuf_, STM32_UART_RX_SIZE, txBuf_, STM32_UART_TX_SIZE),
      huart_(huart)
{
    reg();
}

void Stm32Uart::reg(void) {
    if (regCount_ < STM32_UART_MAX) {
        registry_[regCount_++] = this;
    }
}

Stm32Uart* Stm32Uart::find(UART_HandleTypeDef* huart) {
    for (int i = 0; i < regCount_; ++i) {
        if (registry_[i] && registry_[i]->huart_ == huart) return registry_[i];
    }
    return nullptr;
}

void Stm32Uart::startReceive(void) {
    HAL_UARTEx_ReceiveToIdle_DMA(huart_, rxBuf_, STM32_UART_RX_SIZE);
    __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);
}

void Stm32Uart::restartReceive(void) {
    __HAL_UART_DISABLE_IT(huart_, UART_IT_IDLE);
    HAL_UART_Abort(huart_);
    HAL_UARTEx_ReceiveToIdle_DMA(huart_, rxBuf_, STM32_UART_RX_SIZE);
    __HAL_UART_ENABLE_IT(huart_, UART_IT_IDLE);
}

bool Stm32Uart::startTransmit(const uint8_t* data, uint16_t len) {
    return (HAL_UART_Transmit_DMA(huart_, (uint8_t*)data, len) == HAL_OK);
}

void Stm32Uart::lockCritical(void) { __disable_irq(); }
void Stm32Uart::unlockCritical(void)  { __enable_irq();  }

/* ===================== HAL 全局回调 ===================== */
extern "C" {

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
    Stm32Uart* u = Stm32Uart::find(huart);
    if (u) u->rxEvent(Size);
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
