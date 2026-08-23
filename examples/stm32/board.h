#ifndef EXAMPLES_STM32_BOARD_H
#define EXAMPLES_STM32_BOARD_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 以下为 CubeMX 生成的硬件初始化（示例以 STM32F103xE 为例）。
   真实工程可直接用 CubeMX 生成的 usart.c / gpio.c / dma.c 替换本文件，
   只要保证 huart1 与两个 DMA 句柄存在即可。 */
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_USART1_UART_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* EXAMPLES_STM32_BOARD_H */
