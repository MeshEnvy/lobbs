#pragma once

#include <stddef.h>
#include <stdint.h>

namespace lolog {

static constexpr uint32_t kSectorSize = 4096;
static constexpr uint32_t kPageSize = 256;
static constexpr uint32_t kHeaderSize = 16;
static constexpr uint32_t kInlineMax = 2048;
static constexpr uint32_t kMemtableBytes = 4096;
static constexpr uint32_t kMaxRuns = 4;
static constexpr uint32_t kHeadroomSectors = 2;
static constexpr uint32_t kMagicUsed = 0x4C4F4721; // "LOG!"
static constexpr uint32_t kMagicFree = 0xFFFFFFFF;
static constexpr uint16_t kEntryEnd = 0xFFFF;

static constexpr uint8_t kSegData = 0;
static constexpr uint8_t kSegIndex = 1;

static constexpr uint8_t kEntDir = 1;
static constexpr uint8_t kEntFile = 2;
static constexpr uint8_t kEntExtent = 3;
static constexpr uint8_t kEntTomb = 4;
static constexpr uint8_t kEntCommit = 5;

static constexpr uint8_t kFlagGroupEnd = 0x01;

static constexpr uint32_t kRootDirId = 0;

} // namespace lolog
