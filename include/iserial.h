#ifndef ISERIAL_H
#define ISERIAL_H

#include <cstdint>

class ISerial;  /* 前向声明，供下方函数指针 typedef 使用 */

/* 接收回调：应用层注册它来解析数据，只见到 ISerial&，不碰具体平台 */
typedef void (*SerialRxCb)(ISerial& ser, uint8_t* data, uint16_t len);

/**
 * @brief  串口抽象接口（平台无关）。
 *         FOC / 应用层只依赖这个接口，不依赖 STM32 HAL。
 */
class ISerial {
public:
    virtual ~ISerial() = default;

    virtual void init() = 0;                                  /* 启动接收 */
    virtual int  send(const uint8_t* data, uint16_t len) = 0; /* 非阻塞发送 */

    /* 收到一帧（空闲线）或缓冲满时调用；默认：有回调则转发给回调，否则回发 */
    virtual void onData(uint8_t* data, uint16_t len) {
        if (rxCb_ != nullptr) {
            rxCb_(*this, data, len);
        } else {
            send(data, len);
        }
    }

    void setRxCallback(SerialRxCb cb) { rxCb_ = cb; }

private:
    SerialRxCb rxCb_ = nullptr;
};

#endif /* ISERIAL_H */
