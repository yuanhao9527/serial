/* 冒烟测试用假 HAL：仅提供 stm32_uart.{h,cpp} 所需符号（PC 上运行） */
#ifndef FAKE_STM32_HAL_H
#define FAKE_STM32_HAL_H

#include <stdint.h>

typedef enum {
    HAL_OK      = 0x00U,
    HAL_ERROR   = 0x01U,
    HAL_BUSY    = 0x02U
} HAL_StatusTypeDef;

#define DMA_NORMAL      0U
#define DMA_CIRCULAR    1U

typedef struct { uint32_t Mode; } DMA_InitTypeDef;
typedef struct { DMA_InitTypeDef Init; } DMA_HandleTypeDef;

typedef enum {
    HAL_UART_RXEVENT_TC    = 0x00U,
    HAL_UART_RXEVENT_HT    = 0x01U,
    HAL_UART_RXEVENT_IDLE  = 0x02U
} HAL_UART_RxEventTypeTypeDef;

typedef struct {
    void*              Instance;
    DMA_HandleTypeDef* hdmatx;
    DMA_HandleTypeDef* hdmarx;
    uint32_t           RxEventType;
    /* ---- 测试观测字段（真 HAL 无） ---- */
    int               rxStarts;
    int               rxAborts;
    int               txStarts;
    uint8_t*          rxBuf;     /* 最近一次 ReceiveToIdle 的目标缓冲 */
    uint16_t          rxLen;
    const uint8_t*    txData;    /* 最近一次 Transmit_DMA 的数据 */
    uint16_t          txLen;
} UART_HandleTypeDef;

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef* h, uint8_t* d, uint16_t n);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef* h);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef* h, uint8_t* d, uint16_t n);

uint32_t __get_PRIMASK(void);
void     __disable_irq(void);
void     __set_PRIMASK(uint32_t v);

#endif /* FAKE_STM32_HAL_H */
