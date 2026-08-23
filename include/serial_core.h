#ifndef SERIAL_CORE_H
#define SERIAL_CORE_H

#include "iserial.h"
#include <cstdint>

/**
 * @brief  硬件无关的串口核心实现。
 *         提供：TX 环形缓冲（非阻塞发送）、RX 帧分发、发送完成 / 错误回调。
 *         具体硬件“怎么发 / 怎么收 / 怎么关中断”由子类通过以下纯虚函数提供，
 *         本文件不引用任何 MCU / HAL / 寄存器。
 */
class BufferedSerial : public ISerial {
public:
    BufferedSerial(uint8_t* rxBuf, uint16_t rxSize,
                   uint8_t* txBuf, uint16_t txSize);

    void init(void) override;
    int  send(const uint8_t* data, uint16_t len) override;

    /* ---- 以下由硬件层在相应事件时调用（应用层勿直接调） ---- */
    void onTxComplete(void);                /* 一次异步发送结束 */
    void onRxData(uint8_t* data, uint16_t len); /* 收到一帧 */
    void onError(void);                     /* 接收错误 */

protected:
    /* 子类必须实现：启动一次异步发送，成功返回 true */
    virtual bool writeHardware(const uint8_t* data, uint16_t len) = 0;
    /* 子类必须实现：启动接收（循环 DMA / 中断 等） */
    virtual void startReceiveHardware(void) = 0;
    /* 子类可选：错误后重启接收（默认调用 startReceiveHardware） */
    virtual void restartReceiveHardware(void) { startReceiveHardware(); }

    /* 子类必须实现：进入 / 退出临界区（保护环形缓冲的并发访问） */
    virtual void enterCritical(void) = 0;
    virtual void exitCritical(void) = 0;

private:
    uint8_t*  rxBuf_;
    uint16_t  rxSize_;
    uint8_t*  txBuf_;
    uint16_t  txSize_;

    volatile uint16_t txHead_;
    volatile uint16_t txTail_;
    volatile uint16_t txBusy_;
    volatile uint16_t lastTxLen_;

    void pumpTx(void);
};

#endif /* SERIAL_CORE_H */
