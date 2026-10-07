#pragma once

#include <cstdint>

static constexpr unsigned LO_U64_DEC_LEN = 21;
static constexpr unsigned LO_HUMAN_BYTES_LEN = 8;

/** Writes `value` in decimal into `buf` and returns the first digit. newlib-nano (nRF52) has no `%llu`. */
const char *loU64ToDec(uint64_t value, char (&buf)[LO_U64_DEC_LEN]);

/** Short size like `9B`, `1.2K`, `252K`, `1.8M` (1024-based, integer math). Returns `buf`. */
const char *loHumanBytes(uint64_t bytes, char (&buf)[LO_HUMAN_BYTES_LEN]);
