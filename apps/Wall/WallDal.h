#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <lodb/LoDB.h>
#include <stddef.h>
#include <stdint.h>

static constexpr int LOBBS_WALL_ROWS = 12;
static constexpr int LOBBS_WALL_COLS = 12;
static constexpr int LOBBS_WALL_CELLS = LOBBS_WALL_ROWS * LOBBS_WALL_COLS;

static constexpr uint32_t LOBBS_WALL_DEFAULT_PERIOD_SEC = 3600;
static constexpr uint32_t LOBBS_WALL_DEFAULT_MAX_CELLS = 1;

class WallDal
{
  public:
    explicit WallDal(LoDb &lodb);

    bool formatGridLines(char *out, size_t outCap);
    uint32_t canvasCrc32();
    bool markSeen(uint64_t userUuid, uint32_t crc32);
    bool isDirtyForUser(uint64_t userUuid);
    /** Returns nullptr on success, else a user-facing error. */
    const char *applyPaintTokens(uint64_t userUuid, bool isSysop, const char *const *tokens, int count, uint32_t periodSeconds,
                                 uint32_t maxCellsPerCycle);

  private:
    struct CanvasState {
        char cells[LOBBS_WALL_CELLS + 1];
        uint32_t crc32;
    };

    void loadCanvas(CanvasState &out);
    LoDbError saveCanvas(CanvasState &canvas);
    uint32_t computeCrc32(const uint8_t *data, size_t len);
    uint32_t getLastSeenCrc(uint64_t userUuid);
    LoDb &lodb_;
};

#endif
