#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

/** `yarn_current` (collaborative text is LODB_F_DESCRIPTION). */
namespace YarnCurrentField
{
inline constexpr uint32_t FIELD_TOTAL_WORDS = 0;
} // namespace YarnCurrentField

/** `yarn_seen`. */
namespace YarnSeenField
{
inline constexpr uint32_t FIELD_USER_UUID = 0;
inline constexpr uint32_t FIELD_LAST_TOTAL_WORDS = 1;
} // namespace YarnSeenField

#endif
