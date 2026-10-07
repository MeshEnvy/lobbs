#include "lolog_test_helpers.hpp"
#include "lofs/lolog/LoLogConfig.h"
#include <unity.h>

extern LoLogRamDevice gDev;
extern LoLog gLog;

static uint32_t progCallsForLargeCommit(LoLogRamDevice &dev)
{
    dev.reset();
    LoLog log(dev);
    log.format();
    log.mkdir("gc");
    LoLog::OpenHandle *h = log.open("gc/big.bin", true, true);
    if (!h)
        return 0;
    dev.clearProgCallCount();
    std::vector<uint8_t> payload(2500, 0x5A);
    log.writeHandle(h, payload.data(), payload.size());
    log.flushHandle(h);
    log.closeHandle(h);
    return dev.progCallCount();
}

static bool groupVisibleAfterRemount(LoLogRamDevice &dev, const char *path, size_t expectLen)
{
    LoLog log(dev);
    if (!log.mount())
        return false;
    if (!log.exists(path))
        return expectLen == 0;
    std::vector<uint8_t> got;
    if (!lologReadFile(log, path, got))
        return false;
    return got.size() == expectLen;
}

void test_commit_group_atomic_each_prog_call(void)
{
    const uint32_t calls = progCallsForLargeCommit(gDev);
    TEST_ASSERT_GREATER_THAN(1, calls);

    for (uint32_t fault = 1; fault <= calls; fault++) {
        gDev.reset();
        LoLog log(gDev);
        log.format();
        TEST_ASSERT_TRUE(log.mkdir("gc"));
        gDev.setFaultOnProgCall(fault);
        LoLog::OpenHandle *h = log.open("gc/big.bin", true, true);
        if (h) {
            std::vector<uint8_t> payload(2500, 0x5A);
            log.writeHandle(h, payload.data(), payload.size());
            log.flushHandle(h);
            log.closeHandle(h);
        }
        gDev.setFaultOnProgCall(0);

        const bool full = groupVisibleAfterRemount(gDev, "gc/big.bin", 2500);
        if (!full) {
            LoLog absent(gDev);
            absent.mount();
            if (absent.exists("gc/big.bin")) {
                std::vector<uint8_t> got;
                lologReadFile(absent, "gc/big.bin", got);
                TEST_ASSERT_EQUAL_UINT32(0, got.size());
            }
        } else {
            LoLog check(gDev);
            check.mount();
            std::vector<uint8_t> got;
            lologReadFile(check, "gc/big.bin", got);
            for (uint8_t b : got)
                TEST_ASSERT_EQUAL_UINT8(0x5A, b);
        }
    }
}

void test_commit_group_partial_never_applied(void)
{
    gDev.reset();
    LoLog log(gDev);
    log.format();
    TEST_ASSERT_TRUE(log.mkdir("p"));
    gDev.setFaultOnProgCall(1);
    TEST_ASSERT_FALSE(log.mkdir("p/sub"));
    gDev.setFaultOnProgCall(0);
    LoLog rem(gDev);
    rem.mount();
    TEST_ASSERT_FALSE(rem.exists("p/sub"));
}

void test_commit_group_each_prog_byte(void)
{
    LoLogRamDevice measure(32);
    measure.reset();
    LoLog mlog(measure);
    mlog.format();
    mlog.mkdir("by");
    measure.clearProgBytes();
    std::vector<uint8_t> payload(2500, 0x11);
    LoLog::OpenHandle *mh = mlog.open("by/big.bin", true, true);
    TEST_ASSERT_NOT_NULL(mh);
    mlog.writeHandle(mh, payload.data(), payload.size());
    mlog.flushHandle(mh);
    mlog.closeHandle(mh);
    const uint64_t totalBytes = measure.progBytes();
    TEST_ASSERT_GREATER_THAN(8, totalBytes);

    for (uint64_t cut = 1; cut <= totalBytes; cut++) {
        measure.reset();
        LoLog l(measure);
        l.format();
        l.mkdir("by");
        measure.setFaultAfterProgBytes((uint32_t)cut);
        LoLog::OpenHandle *lh = l.open("by/big.bin", true, true);
        if (lh) {
            l.writeHandle(lh, payload.data(), payload.size());
            l.flushHandle(lh);
            l.closeHandle(lh);
        }
        measure.setFaultAfterProgBytes(0);
        const bool full = groupVisibleAfterRemount(measure, "by/big.bin", 2500);
        if (full) {
            LoLog check(measure);
            check.mount();
            std::vector<uint8_t> got;
            lologReadFile(check, "by/big.bin", got);
            TEST_ASSERT_EQUAL_UINT32(2500, got.size());
        } else {
            LoLog absent(measure);
            absent.mount();
            if (absent.exists("by/big.bin")) {
                std::vector<uint8_t> got;
                lologReadFile(absent, "by/big.bin", got);
                TEST_ASSERT_EQUAL_UINT32(0, got.size());
            }
        }
    }
}

void test_torn_tail_then_append(void)
{
    gDev.reset();
    LoLog log(gDev);
    log.format();
    TEST_ASSERT_TRUE(log.mkdir("torn"));
    TEST_ASSERT_TRUE(lologWriteFile(log, "torn/a.ls", (const uint8_t *)"AAAA", 4));

    gDev.setFaultAfterProgBytes(12);
    LoLog::OpenHandle *h = log.open("torn/b.ls", true, true);
    if (h) {
        log.writeHandle(h, (const uint8_t *)"BBBBBBBB", 8);
        log.flushHandle(h);
        log.closeHandle(h);
    }
    gDev.setFaultAfterProgBytes(0);

    {
        LoLog mid(gDev);
        TEST_ASSERT_TRUE(mid.mount());
        TEST_ASSERT_TRUE(mid.exists("torn/a.ls"));
        if (mid.exists("torn/b.ls")) {
            std::vector<uint8_t> shell;
            lologReadFile(mid, "torn/b.ls", shell);
            TEST_ASSERT_EQUAL_UINT32(0, shell.size());
        }
    }

    LoLog fin(gDev);
    TEST_ASSERT_TRUE(fin.mount());
    TEST_ASSERT_TRUE(lologWriteFile(fin, "torn/c.ls", (const uint8_t *)"CCCC", 4));
    std::vector<uint8_t> finRead;
    TEST_ASSERT_TRUE(lologReadFile(fin, "torn/c.ls", finRead));
    TEST_ASSERT_EQUAL_UINT32(4, finRead.size());
    TEST_ASSERT_TRUE(fin.exists("torn/a.ls"));
}

static void corruptSecondEntryCrc(LoLogRamDevice &dev)
{
    for (uint32_t s = 0; s < dev.sectorCount(); s++) {
        uint32_t off = lolog::kHeaderSize;
        int ent = 0;
        while (off + 8 <= lolog::kSectorSize) {
            uint8_t hdr[8];
            if (!dev.read(s * lolog::kSectorSize + off, hdr, 8))
                break;
            uint16_t plen = (uint16_t)hdr[0] | ((uint16_t)hdr[1] << 8);
            if (plen == lolog::kEntryEnd)
                break;
            const uint32_t padded = (8u + plen + 3u) & ~3u;
            if (hdr[2] == lolog::kEntFile) {
                ent++;
                if (ent == 2 && plen > 0) {
                    dev.poke(s * lolog::kSectorSize + off + 4 + plen, 0x00);
                    return;
                }
            }
            off += padded;
        }
    }
}

void test_torn_entry_crc_poke(void)
{
    gDev.reset();
    LoLog log(gDev);
    log.format();
    TEST_ASSERT_TRUE(log.mkdir("poke"));
    TEST_ASSERT_TRUE(lologWriteFile(log, "poke/ok.ls", (const uint8_t *)"good", 4));
    TEST_ASSERT_TRUE(lologWriteFile(log, "poke/next.ls", (const uint8_t *)"bad", 3));
    corruptSecondEntryCrc(gDev);

    LoLog rem(gDev);
    TEST_ASSERT_TRUE(rem.mount());
    TEST_ASSERT_TRUE(rem.exists("poke/ok.ls"));
    TEST_ASSERT_FALSE(rem.exists("poke/next.ls"));
    TEST_ASSERT_TRUE(lologWriteFile(rem, "poke/after.ls", (const uint8_t *)"yes", 3));
    TEST_ASSERT_TRUE(rem.exists("poke/after.ls"));
}

void test_fault_single_byte_discards_group(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("g"));
    gDev.setFaultAfterProgBytes(1);
    LoLog::OpenHandle *h = gLog.open("g/a.ls", true, true);
    if (h) {
        gLog.writeHandle(h, (const uint8_t *)"x", 1);
        gLog.flushHandle(h);
        gLog.closeHandle(h);
    }
    gDev.setFaultAfterProgBytes(0);
    LoLog g2(gDev);
    TEST_ASSERT_TRUE(g2.mount());
    TEST_ASSERT_FALSE(g2.exists("g/a.ls"));
}
