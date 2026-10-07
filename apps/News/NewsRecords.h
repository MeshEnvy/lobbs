#pragma once
#include <cstdint>

/** App field slots for `news` rows (message body is LODB_F_DESCRIPTION). */
namespace NewsField
{
inline constexpr uint32_t FIELD_AUTHOR = 0;
/** Ephemeral list-row flag (not stored on `news` DB rows). */
inline constexpr uint32_t FIELD_LIST_READ = 1;
} // namespace NewsField

/** App field slots for `news_reads` rows. */
namespace NewsReadField
{
inline constexpr uint32_t FIELD_NEWS_UUID = 0;
inline constexpr uint32_t FIELD_USER_UUID = 1;
} // namespace NewsReadField

