/* 核心层冒烟测试：TX 环形缓冲 / 泵链 / 整帧拒绝 / RX 分发 / 错误恢复
 * 用 MockUart 手动控制「发送完成」时机，覆盖异步排队路径。
 */
#include "serial_core.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("  FAIL line %d: %s\n", __LINE__, #cond); ++failures; } \
} while (0)

class MockUart : public BufferedSerial {
public:
    MockUart() : BufferedSerial(rx_, 64, tx_, 128) {}

    std::vector<std::vector<uint8_t>> chunks;   /* 每次 startTransmit 的数据段 */
    std::vector<uint8_t> wire;                  /* 按序拼接的「线上字节流」 */
    int  receiveStarts = 0;
    bool failStart     = false;

    void complete(void) { onTxComplete(); }

protected:
    bool startTransmit(const uint8_t* d, uint16_t n) override {
        if (failStart) return false;
        chunks.emplace_back(d, d + n);
        wire.insert(wire.end(), d, d + n);
        return true;
    }
    void startReceive(void) override   { ++receiveStarts; }
    void lockCritical(void) override   {}
    void unlockCritical(void) override {}

private:
    uint8_t rx_[64];
    uint8_t tx_[128];
};

/* T1 初始化启动接收 */
static void t1_init(void) {
    printf("[T1] init -> startReceive\n");
    MockUart u;
    u.init();
    CHECK(u.receiveStarts == 1);
}

/* T2 基本发送 + 排队 + 完成驱动 */
static void t2_tx_chain(void) {
    printf("[T2] tx queue & completion chain\n");
    MockUart u;
    u.init();

    CHECK(u.send((const uint8_t*)"hello", 5) == 5);
    CHECK(u.chunks.size() == 1);
    CHECK(u.wire.size() == 5);

    CHECK(u.send((const uint8_t*)"CD", 2) == 2);    /* busy 时仅入队 */
    CHECK(u.chunks.size() == 1);                    /* 未完成不启动新传输 */
    CHECK(u.wire.size() == 5);
    u.complete();                                   /* 恰好一次完成 */
    CHECK(u.chunks.size() == 2);
    CHECK(std::memcmp(u.wire.data(), "helloCD", 7) == 0);
    u.complete();                                   /* 共两次传输，恰好两次完成 */
    CHECK(u.chunks.size() == 2);                    /* 已空 */
}

/* T3 整帧拒绝与精确容量（可用容量 = size-1 = 127） */
static void t3_overflow_reject(void) {
    printf("[T3] full-frame reject at capacity\n");
    MockUart v;
    v.init();

    uint8_t a[50], b[50], c[27];
    std::memset(a, 'A', sizeof a);
    std::memset(b, 'B', sizeof b);
    std::memset(c, 'C', sizeof c);

    CHECK(v.send(a, 50) == 50);
    CHECK(v.wire.size() == 50);                     /* 第一段已上线 */
    CHECK(v.send(b, 50) == 50);                     /* busy：仅入队 */
    CHECK(v.wire.size() == 50);                     /* 排队数据不上线 */
    uint8_t big[28];
    std::memset(big, 'X', sizeof big);
    CHECK(v.send(big, 28) == -1);                   /* 差一个字节：整帧拒绝 */
    CHECK(v.wire.size() == 50);                     /* 未产生任何脏数据 */
    CHECK(v.send(c, 27) == 27);                     /* 恰好装下 free=27 */
    v.complete();                                   /* b+c 连续共 77 字节一次带出 */
    CHECK(v.wire.size() == 127);
    v.complete();                                   /* 共两次传输，恰好两次完成 */
    const std::vector<uint8_t> expectA(50, 'A');
    CHECK(std::memcmp(v.wire.data(), expectA.data(), 50) == 0);
    for (int i = 0; i < 50; ++i) CHECK(v.wire[50 + i] == 'B');
    for (int i = 0; i < 27; ++i) CHECK(v.wire[100 + i] == 'C');
}

/* T4 跨环形边界连续收发不乱序 */
static void t4_wrap_stream(void) {
    printf("[T4] wrap-around stream integrity\n");
    MockUart u;
    u.init();

    std::vector<uint8_t> expect;
    for (int i = 0; i < 20; ++i) {
        uint8_t msg[12];
        for (int j = 0; j < 12; ++j) msg[j] = (uint8_t)('a' + ((i * 12 + j) % 26));
        CHECK(u.send(msg, 12) == 12);
        expect.insert(expect.end(), msg, msg + 12);
        u.complete();
    }
    CHECK(u.wire.size() == expect.size());
    CHECK(u.wire == expect);                        /* 240 字节 > 128，必然跨边界 */
}

/* T5 RX 分发与超长拒收 */
static uint8_t  g_last[256];
static uint16_t g_lastLen = 0xFFFF;
static bool     g_lastFE  = true;
static void rxCap(ISerial& s, uint8_t* d, uint16_t n, bool frameEnd) {
    (void)s;
    memcpy(g_last, d, n);
    g_lastLen = n;
    g_lastFE  = frameEnd;
}
static void t5_rx_dispatch(void) {
    printf("[T5] rx dispatch & oversize reject\n");
    MockUart u;
    u.init();
    u.setRxCallback(rxCap);

    g_lastLen = 0xFFFF;
    u.onRxData((uint8_t*)"abc", 3);
    CHECK(g_lastLen == 3 && memcmp(g_last, "abc", 3) == 0);
    CHECK(g_lastFE == true);                        /* 默认按完整帧 */

    g_lastLen = 0xFFFF;
    u.onRxData((uint8_t*)"xyz", 3, false);
    CHECK(g_lastLen == 3 && memcmp(g_last, "xyz", 3) == 0);
    CHECK(g_lastFE == false);                       /* frameEnd 透传 */

    uint8_t big[65];
    g_lastLen = 0xFFFF;
    u.onRxData(big, 65);                            /* > rxSize(64)：丢弃 */
    CHECK(g_lastLen == 0xFFFF);
}

/* T6 无回调默认回发 */
static void t6_default_echo(void) {
    printf("[T6] default echo without callback\n");
    MockUart u;
    u.init();
    u.onRxData((uint8_t*)"ping", 4);
    CHECK(u.chunks.size() == 1);
    CHECK(std::memcmp(u.wire.data(), "ping", 4) == 0);
}

/* T7 错误重启接收 */
static void t7_error_restart(void) {
    printf("[T7] onError -> restartReceive\n");
    MockUart u;
    u.init();
    int before = u.receiveStarts;
    u.onError();
    CHECK(u.receiveStarts == before + 1);
}

/* T8 启动失败可自愈（数据保留在环中，下次重试） */
static void t8_start_fail_retry(void) {
    printf("[T8] startTransmit failure retry\n");
    MockUart u;
    u.init();
    u.failStart = true;
    CHECK(u.send((const uint8_t*)"x", 1) == 1);     /* 入队但启动失败 */
    CHECK(u.wire.empty());
    u.failStart = false;
    CHECK(u.send((const uint8_t*)"y", 1) == 1);     /* 重试带出旧数据 */
    CHECK(u.wire.size() == 2);
    CHECK(u.wire[0] == 'x' && u.wire[1] == 'y');    /* 顺序保持 */
}

int main(void) {
    t1_init();
    t2_tx_chain();
    t3_overflow_reject();
    t4_wrap_stream();
    t5_rx_dispatch();
    t6_default_echo();
    t7_error_restart();
    t8_start_fail_retry();
    if (failures == 0) printf("CORE SMOKE: ALL PASS\n");
    else               printf("CORE SMOKE: %d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
