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

/* 从 TX 环形缓冲取一段，交给硬件发送 */
void BufferedSerial::pumpTx(void) {
    if (txBusy_) return;

    uint16_t head = txHead_;
    uint16_t tail = txTail_;
    if (head == tail) return;                       /* 空 */

    uint16_t len = (head >= tail) ? (head - tail) : (txSize_ - tail);

    txBusy_   = 1;
    lastTxLen_ = len;
    if (!startTransmit(&txBuf_[tail], len)) {
        txBusy_ = 0;                                /* 启动失败：留待下次 */
    }
}

int BufferedSerial::send(const uint8_t* data, uint16_t len) {
    uint16_t head = txHead_;
    uint16_t tail = txTail_;

    uint16_t free = (tail > head) ? (tail - head - 1)
                                  : (txSize_ - head + tail - 1);
    if (len > free) {
        len = free;                                 /* 空间不够：最多发 free 字节 */
    }
    if (len == 0) return 0;

    lockCritical();
    for (uint16_t i = 0; i < len; ++i) {
        txBuf_[txHead_] = data[i];
        txHead_ = (txHead_ + 1) % txSize_;
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

/* 收到一帧：交给 ISerial 的回调 / 默认回发 */
void BufferedSerial::onRxData(uint8_t* data, uint16_t len) {
    if (len > 0 && len <= rxSize_) {
        onData(data, len);
    }
}

void BufferedSerial::onError(void) {
    restartReceive();
}
