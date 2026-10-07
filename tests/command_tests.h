#pragma once
#include "LoBBSConfig.h"
#if LOBBS_SEED

#include "LoBBSDispatch.h"
#include "LoBBSInstall.h"
#include "LoBBSModule.h"
#include "LoBBSReplyCache.h"
#include "LoBBSSeed.h"
#include "gps/RTC.h"
#include "lofs/LoFS.h"
#include "mesh/NodeDB.h"
#include <cstring>
#include <memory>
#include <string>
#include <unity.h>
#include <vector>

static constexpr uint32_t kLobbsTestBbsNode = 0xBB500001;
static constexpr uint32_t kLobbsTestClientNode = 0xABCD0001;

class LobbsTestNodeDB : public NodeDB
{
  public:
    LobbsTestNodeDB() { myNodeInfo.my_node_num = kLobbsTestBbsNode; }
};

static std::unique_ptr<LobbsTestNodeDB> lobbsTestNodeDb;
static std::unique_ptr<LoBBSModule> lobbsTestModule;
static std::vector<std::string> lobbsTestReplies;

static void lobbsTestSendLine(const char *line)
{
    meshtastic_MeshPacket mp = {};
    mp.from = kLobbsTestClientNode;
    mp.to = kLobbsTestBbsNode;
    size_t len = strlen(line);
    if (len > sizeof(mp.decoded.payload.bytes))
        len = sizeof(mp.decoded.payload.bytes);
    memcpy(mp.decoded.payload.bytes, line, len);
    mp.decoded.payload.size = len;
    lobbsDispatchReceived(lobbsTestModule.get(), mp);
}

static const char *lobbsTestLastReply()
{
    if (lobbsTestReplies.empty())
        return "";
    return lobbsTestReplies.back().c_str();
}

static void lobbsTestFixtureSetUp()
{
    resetRTCStateForTests();
    struct timeval tv = {1000, 0};
    setRTCSystemTimeForTests(&tv);
    lobbsTestNodeDb = std::make_unique<LobbsTestNodeDB>();
    nodeDB = lobbsTestNodeDb.get();
    lobbsTestReplies.clear();
    lobbsTestReplySink = &lobbsTestReplies;
    lobbsTestModule = std::make_unique<LoBBSModule>();
    lobbsInstallAutoSeed(*lobbsTestModule);
    lobbsSeedAll(*lobbsTestModule);
}

static void lobbsTestSendLocalLine(const char *line)
{
    meshtastic_MeshPacket mp = {};
    mp.from = 0;
    mp.to = kLobbsTestBbsNode;
    size_t len = strlen(line);
    memcpy(mp.decoded.payload.bytes, line, len);
    mp.decoded.payload.size = len;
    lobbsDispatchReceived(lobbsTestModule.get(), mp);
}

static void lobbsTestReboot()
{
    lobbsTestModule.reset();
    lobbsTestModule = std::make_unique<LoBBSModule>();
}

static void lobbsTestFixtureTearDown()
{
    lobbsTestReplySink = nullptr;
    lobbsTestReplies.clear();
    lobbsTestModule.reset();
    lobbsTestNodeDb.reset();
    nodeDB = nullptr;
    resetRTCStateForTests();
}

static void test_command_mail_list_and_plain_page2()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 1/"));
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 2/"));
}

static void test_command_machine_page_from_cache()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/42 mail list");
    const char *p1 = lobbsTestLastReply();
    TEST_ASSERT_NOT_NULL(strstr(p1, "<42>ok [1:"));
    lobbsTestSendLine("/43 p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "<43>ok [2:"));
}

static void test_command_no_such_page()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    lobbsTestSendLine("/p99");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such page."));
}

static void test_command_new_command_replaces_cache()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    lobbsTestSendLine("/time");
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such page."));
}

static void test_command_error_keeps_cache()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    lobbsTestSendLine("/mail read 99999");
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "{p 2/"));
}

static void test_command_cache_expires()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/mail list");
    struct timeval tv = {1000 + LOBBS_REPLY_CACHE_TTL_SEC + 1, 0};
    setRTCSystemTimeForTests(&tv);
    lobbsTestSendLine("/p2");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No cached reply."));
}

static void test_command_fs_cwd()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/pwd");
    TEST_ASSERT_EQUAL_STRING("/", lobbsTestLastReply());
    lobbsTestSendLine("/cd /no/such/dir");
    TEST_ASSERT_EQUAL_STRING("No such directory.", lobbsTestLastReply());
    lobbsTestSendLine("/rm relative.txt");
    TEST_ASSERT_EQUAL_STRING("Absolute path only.", lobbsTestLastReply());
    lobbsTestSendLine("/rmtree /flash/.. /flash/..");
    TEST_ASSERT_EQUAL_STRING("Refused.", lobbsTestLastReply());
    lobbsTestSendLine("/cd /flash");
    lobbsTestSendLine("/cd ..");
    TEST_ASSERT_EQUAL_STRING("/", lobbsTestLastReply());
}

static void test_command_fs_mounts_and_tools()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/ls /");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "flash"));
    lobbsTestSendLine("/mkdir /flash/lobbs-test-dir");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Created."));
    lobbsTestSendLine("/stat /flash/lobbs-test-dir");
    TEST_ASSERT_EQUAL_STRING("dir", lobbsTestLastReply());
    lobbsTestSendLine("/mkdir /flash/lobbs-test-dir");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Already exists."));
    lobbsTestSendLine("/df");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "flash: "));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "[lobbs]"));
    lobbsTestSendLine("/ls /flash/lobbs");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "db/"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "install.ls "));
    lobbsTestSendLine("/rmdir /flash/lobbs-test-dir");
}

static void test_command_fs_format_guards()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/format flash");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "This uninstalls LoBBS."));
    lobbsTestSendLine("/format flash 000000");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Wrong or expired code."));
    lobbsTestSendLine("/format nope");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No such mount."));
}

static void test_command_fs_cp_mv()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/cp /flash/lobbs/install.ls /flash/lobbs-copy.ls");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Copied."));
    lobbsTestSendLine("/cp /flash/lobbs/install.ls /flash/lobbs-copy.ls");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Destination exists."));
    lobbsTestSendLine("/mv /flash/lobbs-copy.ls /flash/lobbs-moved.ls");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Moved."));
    lobbsTestSendLine("/rm /flash/lobbs-moved.ls");
}

static void test_command_fs_upload_commit()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/mkdir /flash/tmp");
    lobbsTestSendLine("/rm /flash/tmp/upload-a.bin");
    lobbsTestSendLine("/rm /flash/tmp/upload-b.bin");
    lobbsTestSendLine("/rm /flash/files/upload-done.bin");

    lobbsTestSendLine("/upload /flash/tmp/upload-a.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 0."));

    lobbsTestSendLine("/upload /flash/tmp/upload-a.bin 10:AAAA");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Gap:"));

    lobbsTestSendLine("/upload /flash/tmp/upload-a.bin 0:8xhIpNzLldv25utFy");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));
    lobbsTestSendLine("/upload /flash/tmp/upload-a.bin 0:8xhIpNzLldv25utFy");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));

    lobbsTestSendLine("/commit /flash/tmp/upload-a.bin /flash/files/upload-done.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No CRC in name."));

    lobbsTestSendLine("/upload /flash/tmp/upload-b.bin.01020304.tmp 0:8xhIpNzLldv25utFy");
    lobbsTestSendLine("/commit /flash/tmp/upload-b.bin.01020304.tmp /flash/files/upload-done.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "CRC mismatch."));

    lobbsTestSendLine("/rm /flash/tmp/upload-a.bin");
    lobbsTestSendLine("/rm /flash/tmp/upload-b.bin.01020304.tmp");
}

/** Two sequential chunks: offset 0 then offset == size (append). Payload is "hello world\\n". */
static void test_command_fs_upload_append_two_chunks()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/mkdir /flash/tmp");
    lobbsTestSendLine("/rm /flash/tmp/append-two.bin");

    lobbsTestSendLine("/upload /flash/tmp/append-two.bin 0:0Waqlj8GO");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 6."));
    lobbsTestSendLine("/upload /flash/tmp/append-two.bin 6:0bHyFj1oY");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));

    lobbsTestSendLine("/upload /flash/tmp/append-two.bin 7:0bHyFj1oY");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Gap:"));

    lobbsTestSendLine("/rm /flash/tmp/append-two.bin");
}

static void test_command_fs_upload_commit_ok()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/mkdir /flash/tmp");
    lobbsTestSendLine("/mkdir /flash/files");
    lobbsTestSendLine("/rm /flash/tmp/chunk.ok.af083b2d.tmp");
    lobbsTestSendLine("/rm /flash/files/chunk.ok.bin");

    lobbsTestSendLine("/upload /flash/tmp/chunk.ok.af083b2d.tmp 0:8xhIpNzLldv25utFy");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Size 12."));
    lobbsTestSendLine("/commit /flash/tmp/chunk.ok.af083b2d.tmp /flash/files/chunk.ok.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Moved."));

    lobbsTestSendLine("/cat /flash/files/chunk.ok.bin");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "hello world"));

    lobbsTestSendLine("/rm /flash/files/chunk.ok.bin");
    lobbsTestSendLine("/rm /flash/tmp/chunk.ok.af083b2d.tmp");
}

static void test_command_install_guards()
{
    lobbsTestSendLine("/install flash hacker pass");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Not authorized."));
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/install flash other pass");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Already installed at /flash."));
    lobbsTestSendLine("/login demo99 demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Welcome demo99"));
}

static void test_command_install_autodetect()
{
    lobbsTestReboot();
    lobbsTestSendLine("/login sysop demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Welcome"));
}

static void test_command_install_blank_and_local()
{
    LoFS::rmdir("/flash/lobbs", true);
    lobbsTestReboot();
    lobbsTestSendLine("/whoami");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "/install <flash>"));
    lobbsTestSendLine("/install flash boss pw");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Not authorized."));
    lobbsTestSendLocalLine("/install sd boss pw");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Mount not available."));
    lobbsTestSendLocalLine("/install flash boss pw");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Installed."));
    lobbsTestSendLine("/login newbie pw");
    lobbsTestSendLine("/df");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "SysOp only."));
}

static void test_command_install_adopts_existing()
{
    LoFS::remove("/flash/lobbs/install.ls");
    lobbsTestReboot();
    lobbsTestSendLocalLine("/install flash demo01 demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "sysop credentials required"));
    lobbsTestSendLocalLine("/install flash sysop demo1");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Installed."));
}

static void test_command_users_kick()
{
    lobbsTestSendLine("/login demo03 demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/users kick demo03");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Sessions cleared."));
}

static void test_command_config()
{
    lobbsTestSendLine("/login demo01 demo1");
    lobbsTestSendLine("/config");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "SysOp only."));
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestSendLine("/config");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Use /help config key for details"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max 16"));
    lobbsTestSendLine("/help config session.max");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max: Max concurrent login sessions. Default 16, range 1-64."));
    lobbsTestSendLine("/help config bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No help found for config bogus"));
    lobbsTestSendLine("/config session.max");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max = 16"));
    lobbsTestSendLine("/config session.max 0");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Must be 1-64."));
    lobbsTestSendLine("/config session.max 8");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max = 8."));
    lobbsTestSendLine("/config session.max reset");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "session.max reset."));
    lobbsTestSendLine("/config bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Unknown setting."));
    lobbsTestSendLine("/config password.min 8");
    lobbsTestSendLine("/passwd short short");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Password too short."));
    lobbsTestSendLine("/passwd longenuf longenuf");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Password updated."));
}

static void test_command_subcommand_with_args()
{
    lobbsTestSendLine("/login sysop demo1");
    lobbsTestReplies.clear();
    lobbsTestSendLine("/news read 1");
    TEST_ASSERT_NULL(strstr(lobbsTestLastReply(), "Unknown command"));
    lobbsTestSendLine("/mail send demo01 hello there");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "Mail sent."));
}

static void test_command_help_catalog_and_topics()
{
    lobbsTestSendLine("/help");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), " Help\n"));
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "pN (page N of the last reply)"));
    lobbsTestSendLine("/help bogus");
    TEST_ASSERT_NOT_NULL(strstr(lobbsTestLastReply(), "No help found for bogus"));
    lobbsTestSendLine("/help news read");
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
