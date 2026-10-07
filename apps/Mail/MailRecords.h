#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

/** App field slots for `mail` rows (message body is LODB_F_DESCRIPTION). */
namespace MailField
{
inline constexpr uint32_t FIELD_READ = 0;
inline constexpr uint32_t FIELD_FROM = 1;
inline constexpr uint32_t FIELD_TO = 2;
} // namespace MailField

#endif
