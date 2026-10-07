#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <cstdint>

/** `wall_canvas`. */
namespace WallCanvasField
{
inline constexpr uint32_t FIELD_CELLS = 0;
inline constexpr uint32_t FIELD_CRC32 = 1;
} // namespace WallCanvasField

/** `wall_seen`. */
namespace WallSeenField
{
inline constexpr uint32_t FIELD_USER_UUID = 0;
inline constexpr uint32_t FIELD_CRC32 = 1;
} // namespace WallSeenField

#endif
