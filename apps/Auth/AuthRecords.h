#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

/** App field slots for `users` rows (system fields 95..99 are in LoDB.h). */
namespace AuthUser
{
inline constexpr uint32_t FIELD_USERNAME = 0;
inline constexpr uint32_t FIELD_SYSOP = 1;
inline constexpr uint32_t FIELD_PASSWORD = 2;
} // namespace AuthUser

#endif
