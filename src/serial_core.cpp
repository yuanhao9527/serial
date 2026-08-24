#include "serial_core.h"

BufferedSerial::BufferedSerial(uint8_t* rxBuf, uint16_t rxSize,
                               uint8_t* txBuf, uint16_t txSize)
    : rxBuf_(rxBuf), rxSize_(rxSize),
      txBuf_(txBuf), txSize_(txSize),
      txHead_(0), txTail_(0), txBusy_(0), lastTxLen_(0)
{
}

void BufferedSerial::init(void) {
    startReceive();
}

/* 从 TX 环形缓冲取一段，交给硬件发送。
 * 整个「检查 busy -> 取段 -> 置 busy -> 启动」必须在临界区内完成：
 * pumpTx 可能被 send()（线程/主循环）和 onTxComplete()（中断）并发进入，
 * 否则可能双重启动发送、覆盖 lastTxLen_ 导致环形缓冲错位。 */
void BufferedSerial::pumpTx(void) {
    lockCritical();
    if (!txBusy_ && txHead_ != txTail_) {
        uint16_t len = (txHead_ > txTail_) ? (uint16_t)(txHead_ - txTail_)
                                           : (uint16_t)(txSize_ - txTail_);
        txBusy_   = 1;
        lastTxLen_ = len;
        if (!startTransmit(&txBuf_[txTail_], len)) {
            txBusy_ = 0;                            /* 启动失败：留待下次 */
        }
    }
    unlockCritical();
}

int BufferedSerial::send(const uint8_t* data, uint16_t len) {
    if (len == 0) return 0;

    uint16_t head = txHead_;
    uint16_t tail = txTail_;

    uint16_t free = (tail > head) ? (uint16_t)(tail - head - 1)
                                  : (uint16_t)(txSize_ - head + tail - 1);
    /* 整帧保护：空间不足则整帧拒收（返回 -1），避免发出撕裂的半帧 */
    if (len > free) return -1;

    lockCritical();
    for (uint16_t i = 0; i < len; ++i) {
        txBuf_[txHead_] = data[i];
        txHead_ = (uint16_t)((txHead_ + 1) % txSize_);
    }
    unlockCritical();

    pumpTx();
    return (int)len;
}

/* 一次异步发送完成：推进 tail，继续发剩下的 */
void BufferedSerial::onTxComplete(void) {
    txTail_ = (txTail_ + lastTxLen_) % txSize_;
    txBusy_ = 0;
    pumpTx();
}

/* 收到一段数据：交给 ISerial 的回调 / 默认回发 */
void BufferedSerial::onRxData(uint8_t* data, uint16_t len, bool frameEnd) {
    if (len > 0 && len <= rxSize_) {
        onData(data, len, frameEnd);
    }
}

void BufferedSerial::onError(void) {
    restartReceive();
}
