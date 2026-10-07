#pragma once
#include "core/LoBBSConfig.h"
#if LOBBS_SEED

#include "core/LoBBSInstall.h"
#include "core/LoBBSKernel.h"
#include "core/LoBBSReplyCache.h"
#include "core/LoBBSSeed.h"
#include "lofs/LoFS.h"
#include "tests/meshtastic_fixture.h"
#if LOBBS_ARCH_PORTDUINO
#include "FSCommon.h"
#include "PortduinoFS.h"
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <memory>
#include <string>
#include <unity.h>
#include <vector>

extern std::vector<std::string> *lobbsTestReplySink;

static std::unique_ptr<LobbsTestNodeDB> lobbsTestNodeDb;
static std::unique_ptr<LoBBSKernel> lobbsTestModule;
static std::vector<std::string> lobbsTestReplies;

static const char *lobbsTestLastReply()
{
    if (lobbsTestReplies.empty())
        return "";
    return lobbsTestReplies.back().c_str();
}

#if LOBBS_ARCH_PORTDUINO
static void lobbsTestEnsurePortduinoFs()
{
    static char fsRoot[256];
    snprintf(fsRoot, sizeof(fsRoot), "/tmp/lobbs-cmdtest-%d", (int)getpid());
    mkdir(fsRoot, 0755);
    portduinoVFS->mountpoint(fsRoot);
}
#endif

static void lobbsTestFixtureSetUp()
{
    lobbsTestResetRtc();
#if LOBBS_ARCH_PORTDUINO
    lobbsTestEnsurePortduinoFs();
#endif
    lobbsTestBindNodeDb(lobbsTestNodeDb);
    lobbsTestReplies.clear();
    lobbsTestReplySink = &lobbsTestReplies;
#if LOBBS_ARCH_PORTDUINO
    fsInit();
#endif
    LoFS::resetForTests();
    lobbsTestModule = std::make_unique<LoBBSKernel>();
    lobbsTestModule->begin();
#if LOBBS_ARCH_PORTDUINO
    LoFS::rmdir("/internal/lobbs-test-dir", true);
    LoFS::rmdir("/internal/tmp", true);
#endif
    lobbsInstallAutoSeed(*lobbsTestModule);
    lobbsSeedAll(*lobbsTestModule);
    if (lobbsTestModule->installState() != LoBBSInstallState::Ready) {
        TEST_FAIL_MESSAGE("lobbsInstallAutoSeed did not reach Ready");
        return;
    }
}

static void lobbsTestReboot()
{
    lobbsTestModule.reset();
    LoFS::resetForTests();
    lobbsTestModule = std::make_unique<LoBBSKernel>();
    lobbsTestModule->begin();
}

static void lobbsTestFixtureTearDown()
{
    lobbsTestReplySink = nullptr;
    lobbsTestReplies.clear();
    lobbsTestModule.reset();
    lobbsTestClearNodeDb(lobbsTestNodeDb);
}

static void test_command_mail_list_and_plain_page2()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail list");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 1/"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 2/"));
}

static void test_command_machine_page_from_cache()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/42 mail list");
    const char *p1 = lobbsTestLastReply();
    TEST_ASSERT_NOT_NULL(strstr(p1, "<42>ok [1:"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/43 p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "<43>ok [2:"));
}

static void test_command_no_such_page()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail list");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/p99");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such page."));
}

static void test_command_new_command_replaces_cache()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail list");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/time");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such page."));
}

static void test_command_error_keeps_cache()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail list");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail read 99999");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 2/"));
}

static void test_command_cache_expires()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail list");
    setBootRelativeTimeForUnitTest(1000 + LOBBS_REPLY_CACHE_TTL_SEC + 1);
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No cached reply."));
}

static void test_command_fs_cwd()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/pwd");
    TEST_ASSERT_EQUAL_STRING("/", lobbsTestLastReply());
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/cd /no/such/dir");
    TEST_ASSERT_EQUAL_STRING("No such directory.", lobbsTestLastReply());
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm relative.txt");
    TEST_ASSERT_EQUAL_STRING("Absolute path only.", lobbsTestLastReply());
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rmtree /internal/.. /internal/..");
    TEST_ASSERT_EQUAL_STRING("Refused.", lobbsTestLastReply());
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/cd /internal");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/cd ..");
    TEST_ASSERT_EQUAL_STRING("/", lobbsTestLastReply());
}

static void test_command_fs_mounts_and_tools()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/ls /");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "internal"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mkdir /internal/lobbs-test-dir");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Created."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/stat /internal/lobbs-test-dir");
    TEST_ASSERT_EQUAL_STRING("dir", lobbsTestLastReply());
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mkdir /internal/lobbs-test-dir");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Already exists."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/df");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "internal: "));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "[lobbs]"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/ls /internal/lobbs");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "db/"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "install.ls "));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rmdir /internal/lobbs-test-dir");
}

static void test_command_fs_format_guards()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/format internal");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "This uninstalls LoBBS."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/format internal 000000");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Wrong or expired code."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/format nope");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such mount."));
}

static void test_command_fs_cp_mv()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/cp /internal/lobbs/install.ls /internal/lobbs-copy.ls");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Copied."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/cp /internal/lobbs/install.ls /internal/lobbs-copy.ls");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Destination exists."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mv /internal/lobbs-copy.ls /internal/lobbs-moved.ls");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Moved."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/lobbs-moved.ls");
}

static void test_command_fs_upload_commit()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mkdir /internal/tmp");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Created."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/upload-a.bin");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/upload-b.bin");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/files/upload-done.bin");

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/upload-a.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 0."));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/upload-a.bin 10:AAAA");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Gap:"));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/upload-a.bin 0:8xhIpNzLldv25utFy");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/upload-a.bin 0:8xhIpNzLldv25utFy");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/commit /internal/tmp/upload-a.bin /internal/files/upload-done.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No CRC in name."));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/upload-b.bin.01020304.tmp 0:8xhIpNzLldv25utFy");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/commit /internal/tmp/upload-b.bin.01020304.tmp /internal/files/upload-done.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "CRC mismatch."));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/upload-a.bin");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/upload-b.bin.01020304.tmp");
}

/** Two sequential chunks: offset 0 then offset == size (append). Payload is "hello world\\n". */
static void test_command_fs_upload_append_two_chunks()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mkdir /internal/tmp");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/append-two.bin");

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/append-two.bin 0:0Waqlj8GO");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 6."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/append-two.bin 7:0bHyFj1oY");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Gap:"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/append-two.bin 6:0bHyFj1oY");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/append-two.bin");
}

static void test_command_fs_upload_commit_ok()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mkdir /internal/tmp");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mkdir /internal/files");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/chunk.ok.af083b2d.tmp");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/files/chunk.ok.bin");

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/upload /internal/tmp/chunk.ok.af083b2d.tmp 0:8xhIpNzLldv25utFy");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/commit /internal/tmp/chunk.ok.af083b2d.tmp /internal/files/chunk.ok.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Moved."));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/cat /internal/files/chunk.ok.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "hello world"));

    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/files/chunk.ok.bin");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/rm /internal/tmp/chunk.ok.af083b2d.tmp");
}

static void test_command_install_guards()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/install internal other pass");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Already installed at /internal."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login demo99 demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Welcome demo99"));
}

static void test_command_install_autodetect()
{
    lobbsTestReboot();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Welcome"));
}

static void test_command_install_blank_and_local()
{
    LoFS::rmdir("/internal/lobbs", true);
    lobbsTestReboot();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/whoami");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "/install <internal>"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/install internal boss pw");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Not authorized."));
    lobbsTestSendLocalLine(lobbsTestModule.get(), "/install sd boss pw");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Mount not available."));
    lobbsTestSendLocalLine(lobbsTestModule.get(), "/install internal boss pw");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Installed."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login newbie pw");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/df");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "SysOp only."));
}

static void test_command_install_adopts_existing()
{
    LoFS::remove("/internal/lobbs/install.ls");
    lobbsTestReboot();
    lobbsTestSendLocalLine(lobbsTestModule.get(), "/install internal demo01 demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "sysop credentials required"));
    lobbsTestSendLocalLine(lobbsTestModule.get(), "/install internal sysop demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Installed."));
}

static void test_command_users_kick()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login demo03 demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/users kick demo03");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Sessions cleared."));
}

static void test_command_config()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login demo01 demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "SysOp only."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Use /help config key for details"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max 16"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/help config session.max");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max: Max concurrent login sessions. Default 16, range 1-64."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/help config bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No help found for config bogus"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config session.max");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max = 16"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config session.max 0");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Must be 1-64."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config session.max 8");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max = 8."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config session.max reset");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max reset."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Unknown setting."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/config password.min 8");
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/passwd short short");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Password too short."));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/passwd longenuf longenuf");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Password updated."));
}

static void test_command_subcommand_with_args()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/news read 1");
    TEST_ASSERT_NULL(strstr(lobbsTestLastReply(), "Unknown command"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/mail send demo01 hello there");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Mail sent."));
}

static void test_command_help_catalog_and_topics()
{
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/help");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), " Help\n"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "pN (page N of the last reply)"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/help bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No help found for bogus"));
    lobbsTestSendRadioLine(lobbsTestModule.get(),"/help news read");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "read N"));
    TEST_ASSERT_NULL(strstr(lobbsTestLastReply(), "post message"));
}

inline void lobbsRunCommandTests()
{
    RUN_TEST(test_command_mail_list_and_plain_page2);
    RUN_TEST(test_command_machine_page_from_cache);
    RUN_TEST(test_command_no_such_page);
    RUN_TEST(test_command_new_command_replaces_cache);
    RUN_TEST(test_command_error_keeps_cache);
    RUN_TEST(test_command_cache_expires);
    RUN_TEST(test_command_fs_cwd);
    RUN_TEST(test_command_fs_mounts_and_tools);
    RUN_TEST(test_command_fs_format_guards);
    RUN_TEST(test_command_fs_cp_mv);
    RUN_TEST(test_command_fs_upload_commit);
    RUN_TEST(test_command_fs_upload_append_two_chunks);
    RUN_TEST(test_command_fs_upload_commit_ok);
    RUN_TEST(test_command_install_guards);
    RUN_TEST(test_command_install_autodetect);
    RUN_TEST(test_command_install_blank_and_local);
    RUN_TEST(test_command_install_adopts_existing);
    RUN_TEST(test_command_users_kick);
    RUN_TEST(test_command_config);
    RUN_TEST(test_command_subcommand_with_args);
    RUN_TEST(test_command_help_catalog_and_topics);
}

#endif
