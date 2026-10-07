#include "lolog_test_helpers.hpp"
#include <string>
#include <unity.h>

extern LoLogRamDevice gDev;

extern LoLog gLog;

void test_mkdir_nested_and_overwrite(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("a/b/c"));
    TEST_ASSERT_TRUE(gLog.isDirectory("a"));
    TEST_ASSERT_TRUE(gLog.isDirectory("a/b"));
    TEST_ASSERT_TRUE(gLog.isDirectory("a/b/c"));

    const uint8_t v1[] = "one";
    const uint8_t v2[] = "two-longer";
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "a/b/c/f.ls", v1, sizeof(v1)));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "a/b/c/f.ls", v2, sizeof(v2)));
    std::vector<uint8_t> got;
    TEST_ASSERT_TRUE(lologReadFile(gLog, "a/b/c/f.ls", got));
    TEST_ASSERT_EQUAL_UINT32(sizeof(v2), got.size());
}

void test_rename_file(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("rn"));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "rn/old.ls", (const uint8_t *)"data", 4));
    TEST_ASSERT_TRUE(gLog.rename("rn/old.ls", "rn/new.ls"));
    TEST_ASSERT_FALSE(gLog.exists("rn/old.ls"));
    TEST_ASSERT_TRUE(gLog.exists("rn/new.ls"));
}

void test_rmdir_nonempty_fails_nonempty_ok_recursive(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("rd"));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "rd/x.ls", (const uint8_t *)"1", 1));
    TEST_ASSERT_FALSE(gLog.rmdir("rd", false));
    TEST_ASSERT_TRUE(gLog.rmdir("rd", true));
    TEST_ASSERT_FALSE(gLog.exists("rd/x.ls"));
}

void test_prefix_collision_names(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("pfx"));
    const std::string p1 = "pfx/abcdefgh_11111111.ls";
    const std::string p2 = "pfx/abcdefgh_22222222.ls";
    TEST_ASSERT_TRUE(lologWriteFile(gLog, p1.c_str(), (const uint8_t *)"A", 1));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, p2.c_str(), (const uint8_t *)"B", 1));
    std::vector<uint8_t> a, b;
    TEST_ASSERT_TRUE(lologReadFile(gLog, "pfx/abcdefgh_11111111.ls", a));
    TEST_ASSERT_TRUE(lologReadFile(gLog, "pfx/abcdefgh_22222222.ls", b));
    TEST_ASSERT_EQUAL_UINT8(0x41, a[0]);
    TEST_ASSERT_EQUAL_UINT8(0x42, b[0]);
}

void test_large_file_inline_and_extent(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("big"));
    std::vector<uint8_t> payload(2500);
    for (size_t i = 0; i < payload.size(); i++)
        payload[i] = (uint8_t)(i & 0xFF);
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "big/upload.bin", payload.data(), payload.size()));

    LoLog g2(gDev);
    TEST_ASSERT_TRUE(g2.mount());
    std::vector<uint8_t> got;
    TEST_ASSERT_TRUE(lologReadFile(g2, "big/upload.bin", got));
    TEST_ASSERT_EQUAL_UINT32(payload.size(), got.size());
    TEST_ASSERT_EQUAL_UINT8(0, got[0]);
    TEST_ASSERT_EQUAL_UINT8(0xC3, got[2500 - 1]);
}

void test_large_file_write_at_path(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("up"));
    LoLog::OpenHandle *h = gLog.open("up/chunk.bin", true, true);
    TEST_ASSERT_NOT_NULL(h);
    gLog.closeHandle(h);

    uint8_t chunk[600];
    for (size_t i = 0; i < sizeof(chunk); i++)
        chunk[i] = (uint8_t)i;
    TEST_ASSERT_TRUE(gLog.writeAtPath("up/chunk.bin", 0, chunk, sizeof(chunk)));

    std::vector<uint8_t> got;
    TEST_ASSERT_TRUE(lologReadFile(gLog, "up/chunk.bin", got));
    TEST_ASSERT_EQUAL_UINT32(sizeof(chunk), got.size());
    TEST_ASSERT_EQUAL_UINT8(0, got[0]);
    TEST_ASSERT_EQUAL_UINT8(255, got[255]);
}

void test_stat_and_is_directory(void)
{
    TEST_ASSERT_TRUE(gLog.mkdir("st"));
    TEST_ASSERT_TRUE(lologWriteFile(gLog, "st/f.ls", (const uint8_t *)"x", 1));
    uint32_t sz = 0;
    bool isDir = true;
    TEST_ASSERT_TRUE(gLog.statPath("st/f.ls", &sz, &isDir));
    TEST_ASSERT_FALSE(isDir);
    TEST_ASSERT_EQUAL_UINT32(1, sz);
    TEST_ASSERT_TRUE(gLog.statPath("st", &sz, &isDir));
    TEST_ASSERT_TRUE(isDir);
}
