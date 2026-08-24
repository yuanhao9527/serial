#include "stm32_uart.h"
#include <string.h>
#include <type_traits>
#include <utility>

namespace {

/* 编译期探测 UART_HandleTypeDef 是否含 RxEventType 成员（新版 HAL 才有），
 * 新旧 HAL 均可无配置编译 */
template <typename U>
static auto probeRxEvent(int)
    -> decltype(static_cast<void>(std::declval<U>().RxEventType),
                std::true_type());
template <typename>
static std::false_type probeRxEvent(...);

typedef decltype(probeRxEvent<UART_HandleTypeDef>(0)) HasRxEventType;

} /* namespace */

static inline bool rxEventIsIdle(UART_HandleTypeDef* h, std::true_type) {
    return h->RxEventType == HAL_UART_RXEVENT_IDLE;
}

static inline bool rxEventIsIdle(UART_HandleTypeDef*, std::false_type) {
    return true;                        /* 旧版 HAL 无事件类型：按完整帧上报 */
}

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
    /* ReceiveToIdle 已内部使能 IDLE 中断，无需手动开 */
    HAL_UARTEx_ReceiveToIdle_DMA(huart_, rxBuf_, STM32_UART_RX_SIZE);
}

void Stm32Uart::restartReceive(void) {
    /* 只中止接收：HAL_UART_Abort 会连 TX DMA 一起杀掉，
     * 若错误发生在发送中会让 txBusy_ 永久卡死 */
    HAL_UART_AbortReceive(huart_);
    rxReadIdx_ = 0;
    startReceive();
}

bool Stm32Uart::startTransmit(const uint8_t* data, uint16_t len) {
    return (HAL_UART_Transmit_DMA(huart_, (uint8_t*)data, len) == HAL_OK);
}

/* 保存/恢复式临界区：深度计数支持同实例嵌套；
 * 仅在最外层关中断并保存 PRIMASK，最外层才恢复——
 * 在中断里调用也安全（恢复而非无条件 __enable_irq） */
void Stm32Uart::lockCritical(void) {
    if (++critDepth_ == 1) {
        primaskSave_ = __get_PRIMASK();
        __disable_irq();
    }
}

void Stm32Uart::unlockCritical(void) {
    if (critDepth_ > 0 && --critDepth_ == 0) {
        __set_PRIMASK(primaskSave_);
    }
}

/* ===================== HAL 全局回调 ===================== */

/* 收段位置整理：
 * - 循环 DMA：pos 为环形写指针，跨边界数据拷贝成线性后一次上抛，
 *   避免「从 [0,pos) 当连续数据」导致的乱序错帧；
 * - 普通（NORMAL）DMA：一帧结束后 HAL 已停止接收，这里自动重启；
 * - 帧边界：按 HAL RxEventType 区分空闲线完整帧与缓冲压力拆分。 */
void Stm32Uart::rxEvent(uint16_t pos) {
    bool circular = (huart_->hdmarx != nullptr) &&
                    (huart_->hdmarx->Init.Mode == DMA_CIRCULAR);

    /* 溢出防护：写位置距未读位置越过保留区 = 回调被饿死、
     * 未读数据已被覆盖。丢弃整个会话并重启，计数可观测。 */
    uint16_t dist = (uint16_t)((pos + STM32_UART_RX_SIZE - rxReadIdx_) %
                               STM32_UART_RX_SIZE);
    if (dist > (uint16_t)(STM32_UART_RX_SIZE - STM32_UART_RX_RESERVE)) {
        ++rxOverruns_;
        restartReceive();
        return;
    }

    uint16_t total;
    if (pos >= rxReadIdx_) {
        total = (uint16_t)(pos - rxReadIdx_);
        if (total > 0) {
            memcpy(rxLinear_, &rxBuf_[rxReadIdx_], total);
        }
    } else {                                        /* 跨越缓冲末尾的回卷 */
        uint16_t first = (uint16_t)(STM32_UART_RX_SIZE - rxReadIdx_);
        total = (uint16_t)(first + pos);
        if (first > 0) {
            memcpy(rxLinear_, &rxBuf_[rxReadIdx_], first);
        }
        if (pos > 0) {
            memcpy(&rxLinear_[first], &rxBuf_[0], pos);
        }
    }
    rxReadIdx_ = pos;

    /* 帧边界：新版 HAL 由 RxEventType 给出；旧版无此成员则按完整帧 */
    bool frameEnd = rxEventIsIdle(huart_, HasRxEventType());

    if (total > 0) {
        onRxData(rxLinear_, total, frameEnd);
    }
    if (!circular) {
        restartReceive();
    }
}

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
