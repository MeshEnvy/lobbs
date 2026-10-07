#include "lolog_test_helpers.hpp"
#include "lofs/lolog/LoLogConfig.h"
#include <unity.h>

extern LoLogRamDevice gDev;
extern LoLog gLog;

void test_index_flush_pure_reboot(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("ix"));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "ix/a.ls", (const uint8_t *)"z", 1));
    gLog.maintain(60000);

    LoLog g2(gDev);
    TEST_ASSERT_TRUE(g2.mount());
    TEST_ASSERT_TRUE(g2.exists("ix/a.ls"));
}

void test_index_multiple_flushes_and_lookup(void)
{
    for (int i = 0; i < 90; i++) {
        char dir[16];
        snprintf(dir, sizeof(dir), "bulk%d", i);
        TEST_ASSERT_TRUE(gLog.mkdir(dir));
        char path[32];
        snprintf(path, sizeof(path), "%s/r.ls", dir);
        TEST_ASSERT_TRUE(lologWriteFile(gLog, path, (const uint8_t *)"x", 1));
        gLog.maintain(60000);
    }
    LoLog g2(gDev);
    TEST_ASSERT_TRUE(g2.mount());
    TEST_ASSERT_TRUE(g2.exists("bulk42/r.ls"));
    TEST_ASSERT_TRUE(g2.exists("bulk0/r.ls"));
}

void test_tombstone_shadow_after_remount(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("ts"));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "ts/gone.ls", (const uint8_t *)"1", 1));
    TEST_ASSERT_TRUE(gLog.remove("ts/gone.ls"));
    gLog.maintain(60000);

    LoLog g2(gDev);
    TEST_ASSERT_TRUE(g2.mount());
    TEST_ASSERT_FALSE(g2.exists("ts/gone.ls"));
}

void test_delete_when_near_full(void)
{
    LoLogRamDevice tiny(16);
    LoLog log(tiny);
    log.format();
    TEST_ASSERT_TRUE(lologWriteFile(log, "f0.ls", (const uint8_t *)"x", 1));
    int created = 1;
    for (int i = 1; i < 500; i++) {
        char path[24];
        snprintf(path, sizeof(path), "f%d.ls", i);
        if (!lologWriteFile(log, path, (const uint8_t *)"x", 1))
            break;
        created++;
    }
    TEST_ASSERT_GREATER_THAN(3, created);
    TEST_ASSERT_TRUE(log.remove("f0.ls"));
    char extra[24];
    snprintf(extra, sizeof(extra), "f%d.ls", created + 10);
    TEST_ASSERT_TRUE(lologWriteFile(log, extra, (const uint8_t *)"n", 1));
}

void test_cleaner_recycles_sectors(void)
{
    LoLogRamDevice dev(12);
    LoLog log(dev);
    log.format();
    for (int i = 0; i < 80; i++) {
        char path[20];
        snprintf(path, sizeof(path), "c%d.ls", i);
        if (!lologWriteFile(log, path, (const uint8_t *)"xy", 2))
            break;
    }
    for (int i = 0; i < 40; i++) {
        char path[20];
        snprintf(path, sizeof(path), "c%d.ls", i);
        log.remove(path);
    }
    for (int m = 0; m < 60; m++)
        log.maintain(60000);
    uint32_t usedAfter = (uint32_t)log.usedBytes();
    (void)usedAfter;
    TEST_ASSERT_TRUE(log.exists("c79.ls"));
    std::vector<uint8_t> tail;
    TEST_ASSERT_TRUE(lologReadFile(log, "c79.ls", tail));
    TEST_ASSERT_EQUAL_UINT8(0x78, tail[0]);
}

void test_headroom_blocks_before_starve(void)
{
    LoLogRamDevice tiny(6);
    LoLog log(tiny);
    log.format();
    for (int i = 0; i < 200; i++) {
        char path[32];
        snprintf(path, sizeof(path), "f%d.ls", i);
        LoLog::OpenHandle *h = log.open(path, true, true);
        if (!h)
            break;
        log.closeHandle(h);
    }
    LoLog::OpenHandle *last = log.open("overflow.ls", true, true);
    TEST_ASSERT_NULL(last);
}
