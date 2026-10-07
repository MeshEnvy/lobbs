#include "lofs/lolog/LoLogRamDevice.h"
#include "lofs/lolog/LoLog.h"
#include <unity.h>

LoLogRamDevice gDev(32);
LoLog gLog(gDev);

void setUp(void)
{
    gDev.reset();
    gLog.format();
}

void tearDown(void) {}

#define DECL(name) void name(void)

DECL(test_mount_empty_after_format);
DECL(test_mount_erased_chip_without_format);
DECL(test_pure_replay_no_index_runs);
DECL(test_mount_after_reboot_simulation);

DECL(test_mkdir_nested_and_overwrite);
DECL(test_rename_file);
DECL(test_rmdir_nonempty_fails_nonempty_ok_recursive);
DECL(test_prefix_collision_names);
DECL(test_large_file_inline_and_extent);
DECL(test_large_file_write_at_path);
DECL(test_stat_and_is_directory);

DECL(test_commit_group_atomic_each_prog_call);
DECL(test_commit_group_each_prog_byte);
DECL(test_torn_tail_then_append);
DECL(test_torn_entry_crc_poke);
DECL(test_fault_single_byte_discards_group);
DECL(test_commit_group_partial_never_applied);

DECL(test_index_flush_pure_reboot);
DECL(test_index_multiple_flushes_and_lookup);
DECL(test_tombstone_shadow_after_remount);
DECL(test_delete_when_near_full);
DECL(test_cleaner_recycles_sectors);
DECL(test_headroom_blocks_before_starve);

DECL(test_random_ops_with_crash_injection);
DECL(test_directory_list_count);

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_mount_empty_after_format);
    RUN_TEST(test_mount_erased_chip_without_format);
    RUN_TEST(test_pure_replay_no_index_runs);
    RUN_TEST(test_mount_after_reboot_simulation);

    RUN_TEST(test_mkdir_nested_and_overwrite);
    RUN_TEST(test_rename_file);
    RUN_TEST(test_rmdir_nonempty_fails_nonempty_ok_recursive);
    RUN_TEST(test_prefix_collision_names);
    RUN_TEST(test_large_file_inline_and_extent);
    RUN_TEST(test_large_file_write_at_path);
    RUN_TEST(test_stat_and_is_directory);

    RUN_TEST(test_commit_group_atomic_each_prog_call);
    RUN_TEST(test_commit_group_each_prog_byte);
    RUN_TEST(test_torn_tail_then_append);
    RUN_TEST(test_torn_entry_crc_poke);
    RUN_TEST(test_fault_single_byte_discards_group);
    RUN_TEST(test_commit_group_partial_never_applied);

    RUN_TEST(test_index_flush_pure_reboot);
    RUN_TEST(test_index_multiple_flushes_and_lookup);
    RUN_TEST(test_tombstone_shadow_after_remount);
    RUN_TEST(test_delete_when_near_full);
    RUN_TEST(test_cleaner_recycles_sectors);
    RUN_TEST(test_headroom_blocks_before_starve);

    RUN_TEST(test_random_ops_with_crash_injection);
    RUN_TEST(test_directory_list_count);

    return UNITY_END();
}
