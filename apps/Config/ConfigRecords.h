#pragma once
#include <cstdint>

/** Stored override row in table `config`. */
namespace ConfigStoreField
{
inline constexpr uint32_t FIELD_VALUE = 0;
} // namespace ConfigStoreField

/** Key definition rows from `config_keys`. */
namespace ConfigKeyField
{
inline constexpr uint32_t FIELD_DEFAULT = 0;
inline constexpr uint32_t FIELD_MIN = 1;
inline constexpr uint32_t FIELD_MAX = 2;
} // namespace ConfigKeyField

/** Proposed value in `config_validate` value record. */
namespace ConfigValidateField
{
inline constexpr uint32_t FIELD_VALUE = 0;
} // namespace ConfigValidateField

