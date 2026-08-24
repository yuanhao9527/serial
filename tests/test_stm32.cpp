/* STM32 端口冒烟测试（假 HAL，PC 上运行）：
 * 覆盖 回卷拆帧 / 普通模式自动重启 / 错误恢复 / TX 完成链 / 临界区嵌套。
 */
#include "stm32_uart.h"
#include <cstdio>
#include <cstring>

/* stm32_uart.cpp 中定义的 HAL 全局回调（C 链接） */
extern "C" {
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart);
}

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("  FAIL line %d: %s\n", __LINE__, #cond); ++failures; } \
} while (0)

/* ---- 假 HAL 实现（带观测计数） ---- */
static uint32_t g_primask = 0;
uint32_t __get_PRIMASK(void)          { return g_primask; }
void     __disable_irq(void)          { g_primask = 1; }
void     __set_PRIMASK(uint32_t v)    { g_primask = v; }

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef* h, uint8_t* d, uint16_t n) {
    h->rxStarts++; h->rxBuf = d; h->rxLen = n;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef* h) {
    h->rxAborts++;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef* h, uint8_t* d, uint16_t n) {
    h->txStarts++; h->txData = d; h->txLen = n;
    return HAL_OK;
}

/* ---- 应用回调捕获 ---- */
static uint8_t  g_last[256];
static uint16_t g_lastLen = 0xFFFF;
static bool     g_lastFE  = true;
static void cap(ISerial& s, uint8_t* d, uint16_t n, bool frameEnd) {
    (void)s;
    memcpy(g_last, d, n);
    g_lastLen = n;
    g_lastFE  = frameEnd;
}

/* 暴露 protected 临界区以便直接测试 */
class Probe : public Stm32Uart {
public:
    explicit Probe(UART_HandleTypeDef* h) : Stm32Uart(h) {}
    using Stm32Uart::lockCritical;
    using Stm32Uart::unlockCritical;
};

int main(void) {
    DMA_HandleTypeDef dCirc{};   dCirc.Init.Mode  = DMA_CIRCULAR;
    DMA_HandleTypeDef dNorm{};   dNorm.Init.Mode  = DMA_NORMAL;
    UART_HandleTypeDef h1{};     h1.hdmarx = &dCirc;
    UART_HandleTypeDef h2{};     h2.hdmarx = &dNorm;
    UART_HandleTypeDef h3{};     h3.hdmarx = &dCirc;

    Stm32Uart u1(&h1);
    Stm32Uart u2(&h2);
    Probe     p3(&h3);
    u1.setRxCallback(cap);
    u2.setRxCallback(cap);

    /* S1 循环模式：init 启动接收 + 线性帧 */
    printf("[S1] circular: init + linear frame\n");
    u1.init();
    CHECK(h1.rxStarts == 1 && h1.rxBuf != nullptr && h1.rxLen == 64);
    memcpy(h1.rxBuf, "HELLO", 5);
    h1.RxEventType = HAL_UART_RXEVENT_IDLE;
    HAL_UARTEx_RxEventCallback(&h1, 5);
    CHECK(g_lastLen == 5 && memcmp(g_last, "HELLO", 5) == 0);
    CHECK(g_lastFE == true);                        /* 空闲线 -> 完整帧 */
    CHECK(h1.rxStarts == 1);                        /* 循环模式不重启 */

    /* S2 循环回卷：按真实事件序列（HT 半满 -> IDLE 空闲）跨末尾累积。
     * HT/TC 保证回调间隔 ≤ 半缓冲，单次事件跳 60 字节会被溢出防护正确拦截 */
    printf("[S2] circular: accumulated segments across end\n");
    for (int i = 5; i < 37; ++i) h1.rxBuf[i] = (uint8_t)i;
    h1.RxEventType = HAL_UART_RXEVENT_HT;
    HAL_UARTEx_RxEventCallback(&h1, 37);            /* 读指针 5 -> 37 */
    CHECK(g_lastLen == 32 && g_last[0] == 5 && g_last[31] == 36);
    CHECK(g_lastFE == false);
    for (int i = 37; i < 64; ++i) h1.rxBuf[i] = (uint8_t)i;
    h1.rxBuf[0] = 0x77;
    h1.RxEventType = HAL_UART_RXEVENT_IDLE;
    HAL_UARTEx_RxEventCallback(&h1, 1);             /* 回卷：37..63 + [0] */
    CHECK(g_lastLen == 28);
    CHECK(g_last[0] == 37 && g_last[26] == 63 && g_last[27] == 0x77);
    CHECK(g_lastFE == true);
    CHECK(h1.rxStarts == 1);

    /* S3 整圈满（TC pos==size，读指针 -> 64）后继续回卷 */
    printf("[S3] circular: full-circle then wrap again\n");
    for (int i = 1; i < 33; ++i) h1.rxBuf[i] = (uint8_t)i;
    h1.RxEventType = HAL_UART_RXEVENT_HT;
    HAL_UARTEx_RxEventCallback(&h1, 33);            /* 读指针 1 -> 33 */
    CHECK(g_lastLen == 32 && g_last[0] == 1 && g_last[31] == 32);
    for (int i = 33; i < 64; ++i) h1.rxBuf[i] = (uint8_t)i;
    h1.RxEventType = HAL_UART_RXEVENT_TC;
    HAL_UARTEx_RxEventCallback(&h1, 64);            /* 整圈事件 */
    CHECK(g_lastLen == 31 && g_last[0] == 33 && g_last[30] == 63);
    CHECK(g_lastFE == false);
    h1.rxBuf[0] = 0xAA; h1.rxBuf[1] = 0xBB;
    h1.RxEventType = HAL_UART_RXEVENT_IDLE;
    HAL_UARTEx_RxEventCallback(&h1, 2);             /* 64 -> 回卷到 2 */
    CHECK(g_lastLen == 2 && g_last[0] == 0xAA && g_last[1] == 0xBB);
    CHECK(g_lastFE == true);

    /* S4 普通模式：每帧自动重启，且读指针归零 */
    printf("[S4] normal mode: auto restart per frame\n");
    u2.init();
    CHECK(h2.rxStarts == 1);
    memcpy(h2.rxBuf, "PING", 4);
    h2.RxEventType = HAL_UART_RXEVENT_IDLE;
    HAL_UARTEx_RxEventCallback(&h2, 4);
    CHECK(g_lastLen == 4 && memcmp(g_last, "PING", 4) == 0);
    CHECK(g_lastFE == true);
    CHECK(h2.rxStarts == 2 && h2.rxAborts == 1);    /* AbortReceive + 重启 */
    memcpy(h2.rxBuf, "OK!", 3);                     /* 新会话从 0 开始写 */
    HAL_UARTEx_RxEventCallback(&h2, 3);
    CHECK(g_lastLen == 3 && memcmp(g_last, "OK!", 3) == 0);  /* 读指针已复位 */

    /* S5 错误恢复：只动接收路径 */
    printf("[S5] error callback recovery\n");
    int starts = h2.rxStarts, aborts = h2.rxAborts;
    HAL_UART_ErrorCallback(&h2);
    CHECK(h2.rxStarts == starts + 1 && h2.rxAborts == aborts + 1);

    /* S6 TX：完成中断推进队列 */
    printf("[S6] tx completion chain\n");
    CHECK(u2.send((const uint8_t*)"Q", 1) == 1);
    CHECK(h2.txStarts == 1 && h2.txLen == 1 && h2.txData[0] == 'Q');
    CHECK(u2.send((const uint8_t*)"R", 1) == 1);    /* busy 入队 */
    CHECK(h2.txStarts == 1);
    HAL_UART_TxCpltCallback(&h2);
    CHECK(h2.txStarts == 2 && h2.txData[0] == 'R');

    /* S8 帧边界：HT 型事件 frameEnd=false，IDLE 型 true
     * （u1 读指针此时为 S3 结束后的 2） */
    printf("[S8] frame boundary via RxEventType\n");
    memcpy(&h1.rxBuf[2], "HTHT", 4);
    h1.RxEventType = HAL_UART_RXEVENT_HT;
    HAL_UARTEx_RxEventCallback(&h1, 6);
    CHECK(g_lastLen == 4 && memcmp(g_last, "HTHT", 4) == 0);
    CHECK(g_lastFE == false);
    h1.RxEventType = HAL_UART_RXEVENT_IDLE;
    HAL_UARTEx_RxEventCallback(&h1, 6);             /* 无新数据：不上抛 */
    CHECK(g_lastLen == 4);                          /* 回调未被再次触发 */

    /* S9 溢出防护：写位置越过保留区 -> 丢弃会话重启接收并计数 */
    printf("[S9] overrun guard\n");
    uint16_t prevLen = g_lastLen;
    CHECK(u1.rxOverruns() == 0);
    h1.RxEventType = HAL_UART_RXEVENT_HT;
    HAL_UARTEx_RxEventCallback(&h1, 56);            /* dist=(56-6)=50 > 64-16 触发 */
    CHECK(u1.rxOverruns() == 1);
    CHECK(h1.rxStarts == 2 && h1.rxAborts == 1);    /* 已重启，读指针归零 */
    CHECK(g_lastLen == prevLen);                    /* 脏数据未上抛 */
    memcpy(h1.rxBuf, "OK", 2);                      /* 新会话正常收帧 */
    h1.RxEventType = HAL_UART_RXEVENT_IDLE;
    HAL_UARTEx_RxEventCallback(&h1, 2);
    CHECK(g_lastLen == 2 && memcmp(g_last, "OK", 2) == 0);

    /* S7 临界区：同实例可嵌套（深度计数），且 ISR 内不会提前开中断 */
    printf("[S7] nested critical section\n");
    g_primask = 0;
    p3.lockCritical();                              /* 外层：保存 0 并关中断 */
    CHECK(g_primask == 1);
    g_primask = 1;                                  /* 模拟外层期间进入 ISR */
    p3.lockCritical();                              /* 内层：不覆盖保存值 */
    p3.unlockCritical();
    CHECK(g_primask == 1);                          /* 不提前恢复 */
    p3.unlockCritical();                            /* 最外层才真正恢复 */
    g_primask = 0;

    g_primask = 1;                                  /* 模拟纯 ISR 上下文 */
    p3.lockCritical();
    p3.unlockCritical();
    CHECK(g_primask == 1);                          /* 退出仍是关中断，不被误开 */
    g_primask = 0;

    if (failures == 0) printf("STM32 PORT SMOKE: ALL PASS\n");
    else               printf("STM32 PORT SMOKE: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
