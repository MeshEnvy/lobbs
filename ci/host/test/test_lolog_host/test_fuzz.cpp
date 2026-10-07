#include "lolog_test_helpers.hpp"
#include "ref_fs.hpp"
#include <unity.h>

extern LoLogRamDevice gDev;

static uint32_t lfsRand(uint32_t &state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

static void applyRefOp(RefFs &ref, LoLog &log, uint32_t op, uint32_t &rng)
{
    char path[48];
    char path2[48];
    switch (op % 5) {
    case 0: {
        snprintf(path, sizeof(path), "fuzz/d%d", (int)(lfsRand(rng) % 20));
        if (log.mkdir(path))
            ref.mkdir(path);
        break;
    }
    case 1: {
        snprintf(path, sizeof(path), "fuzz/f%d.dat", (int)(lfsRand(rng) % 40));
        std::vector<uint8_t> data(8 + (lfsRand(rng) % 120));
        for (size_t i = 0; i < data.size(); i++)
            data[i] = (uint8_t)lfsRand(rng);
        if (lologWriteFile(log, path, data.data(), data.size()))
            ref.write(path, data);
        break;
    }
    case 2: {
        snprintf(path, sizeof(path), "fuzz/f%d.dat", (int)(lfsRand(rng) % 40));
        if (log.remove(path))
            ref.unlink(path);
        break;
    }
    case 3: {
        snprintf(path, sizeof(path), "fuzz/f%d.dat", (int)(lfsRand(rng) % 40));
        snprintf(path2, sizeof(path2), "fuzz/t%d.dat", (int)(lfsRand(rng) % 40));
        if (ref.existsFile(path) && !ref.existsFile(path2) && log.rename(path, path2))
            ref.rename(path, path2);
        break;
    }
    default:
        log.maintain(5000);
        break;
    }
}

void test_random_ops_with_crash_injection(void)
{
    RefFs ref;
    uint32_t rng = 0xC0FFEE42u;

    for (int round = 0; round < 30; round++) {
        gDev.reset();
        LoLog log(gDev);
        log.format();
        log.mkdir("fuzz");
        ref.clear();
        ref.mkdir("fuzz");

        const int opsBeforeCrash = 3 + (int)(lfsRand(rng) % 12);
        for (int i = 0; i < opsBeforeCrash; i++)
            applyRefOp(ref, log, lfsRand(rng), rng);

        gDev.setFaultOnProgCall(1 + (lfsRand(rng) % 8));
        applyRefOp(ref, log, lfsRand(rng), rng);
        gDev.setFaultOnProgCall(0);

        LoLog rem(gDev);
        TEST_ASSERT_TRUE(rem.mount());

        for (const auto &kv : ref.allFiles()) {
            if (!rem.exists(kv.first.c_str()))
                continue;
            std::vector<uint8_t> got;
            if (!lologReadFile(rem, kv.first.c_str(), got))
                continue;
            if (got.size() != kv.second.size())
                continue;
            if (!got.empty())
                TEST_ASSERT_EQUAL_UINT8_ARRAY(kv.second.data(), got.data(), got.size());
        }
    }
}

void test_directory_list_count(void)
{
    gDev.reset();
    LoLog log(gDev);
    log.format();
    TEST_ASSERT_TRUE(log.mkdir("d"));
    TEST_ASSERT_TRUE(lologWriteFile(log, "d/one.ls", (const uint8_t *)"1", 1));
    TEST_ASSERT_TRUE(lologWriteFile(log, "d/two.ls", (const uint8_t *)"2", 1));
    TEST_ASSERT_EQUAL(2, lologCountDir(log, "d"));
}
