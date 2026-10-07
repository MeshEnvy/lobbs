#include "lolog_test_helpers.hpp"
#include <unity.h>

extern LoLogRamDevice gDev;
extern LoLog gLog;

void test_mount_empty_after_format(void)
{
    TEST_ASSERT_FALSE(gLog.exists("anything"));
    TEST_ASSERT_TRUE(gLog.isDirectory(""));
}

void test_mount_erased_chip_without_format(void)
{
    LoLogRamDevice dev(16);
    LoLog log(dev);
    TEST_ASSERT_TRUE(lologMountErasedChip(dev, log));
}

void test_pure_replay_no_index_runs(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("replay"));
    std::vector<uint8_t> payload = {'a', 'b', 'c'};
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "replay/r1.ls", payload.data(), payload.size()));

    LoLog g2(gDev);
    TEST_ASSERT_TRUE(g2.mount());
    std::vector<uint8_t> readback;
    TEST_ASSERT_TRUE(lologReadFile(g2, "replay/r1.ls", readback));
    TEST_ASSERT_EQUAL_UINT32(payload.size(), readback.size());
}

void test_mount_after_reboot_simulation(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("boot"));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "boot/persist.ls", (const uint8_t *)"ok", 2));

    LoLog after(gDev);
    TEST_ASSERT_TRUE(after.mount());
    TEST_ASSERT_TRUE(after.exists("boot/persist.ls"));
}
