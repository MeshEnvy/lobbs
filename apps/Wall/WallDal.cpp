#include "WallDal.h"
#include "../AppUtil.h"
#include "WallRecords.h"
#include <cctype>
#include <cstdio>
#include <cstring>

#include "core/LoBBSStackGuard.h"

WallDal::WallDal(LoDb &lodb) : lodb_(lodb)
{
    lodb_.registerTable("wall_canvas");
    lodb_.registerTable("wall_seen");
    lodb_.registerTable("wall_quota");
}

static lodb_uuid_t wallCanvasRecordUuid()
{
    return lodb_new_uuid("lobbs_wall_canvas_v1", 0);
}

static lodb_uuid_t wallSeenRecordUuid(uint64_t userUuid)
{
    char hex[17];
    lodb_uuid_to_hex(userUuid, hex);
    return lodb_new_uuid(hex, 0);
}

static void wallSealCells(char *cells)
{
    cells[LOBBS_WALL_CELLS] = '\0';
}

uint32_t WallDal::computeCrc32(const uint8_t *data, size_t len)
{
    return lobbsCrc32(data, len);
}

void WallDal::loadCanvas(CanvasState &out)
{
    lodb_uuid_t id = wallCanvasRecordUuid();
    LoScalar rec;
    if (lodb_.get("wall_canvas", id, rec) == LODB_OK) {
        std::string cells;
        if (rec.getString(WallCanvasField::FIELD_CELLS, cells))
            strncpy(out.cells, cells.c_str(), LOBBS_WALL_CELLS);
        else
            memset(out.cells, ' ', LOBBS_WALL_CELLS);
        wallSealCells(out.cells);
        rec.getUint32(WallCanvasField::FIELD_CRC32, out.crc32);
        return;
    }
    memset(out.cells, ' ', LOBBS_WALL_CELLS);
    wallSealCells(out.cells);
    out.crc32 = computeCrc32((const uint8_t *)out.cells, LOBBS_WALL_CELLS);
}

LoDbError WallDal::saveCanvas(CanvasState &canvas)
{
    wallSealCells(canvas.cells);
    canvas.crc32 = computeCrc32((const uint8_t *)canvas.cells, LOBBS_WALL_CELLS);
    lodb_uuid_t id = wallCanvasRecordUuid();
    LoScalar rec;
    rec.setString(WallCanvasField::FIELD_CELLS, canvas.cells);
    rec.setUint32(WallCanvasField::FIELD_CRC32, canvas.crc32);
    return lodb_.upsert("wall_canvas", id, rec);
}

bool WallDal::formatGridLines(char *out, size_t outCap)
{
    CanvasState canvas = {};
    loadCanvas(canvas);
    size_t n = 0;
    for (int row = 0; row < LOBBS_WALL_ROWS; row++) {
        if (row > 0 && n + 1 < outCap)
            out[n++] = '\n';
        for (int col = 0; col < LOBBS_WALL_COLS; col++) {
            if (n + 1 >= outCap)
                return false;
            out[n++] = canvas.cells[row * LOBBS_WALL_COLS + col];
        }
    }
    if (n >= outCap)
        return false;
    out[n] = '\0';
    return true;
}

uint32_t WallDal::canvasCrc32()
{
    CanvasState canvas = {};
    loadCanvas(canvas);
    return canvas.crc32;
}

uint32_t WallDal::getLastSeenCrc(uint64_t userUuid)
{
    LoScalar seen;
    if (lodb_.get("wall_seen", wallSeenRecordUuid(userUuid), seen) != LODB_OK)
        return 0xffffffff;
    uint32_t crc = 0;
    seen.getUint32(WallSeenField::FIELD_CRC32, crc);
    return crc;
}

bool WallDal::markSeen(uint64_t userUuid, uint32_t crc32Val)
{
    LoScalar seen;
    seen.setUint64(WallSeenField::FIELD_USER_UUID, userUuid);
    seen.setUint32(WallSeenField::FIELD_CRC32, crc32Val);
    lodb_uuid_t id = wallSeenRecordUuid(userUuid);
    return lodb_.upsert("wall_seen", id, seen) == LODB_OK;
}

bool WallDal::isDirtyForUser(uint64_t userUuid)
{
    uint32_t cur = canvasCrc32();
    uint32_t last = getLastSeenCrc(userUuid);
    if (last == 0xffffffff)
        return true;
    return cur != last;
}

static bool parseCoord(const char *tok, int &rowOut, int &colOut, const char **endOut)
{
    if (!tok || !tok[0])
        return false;
    char r = (char)tolower((unsigned char)tok[0]);
    if (r < 'a' || r > 'l')
        return false;
    rowOut = r - 'a';
    const char *p = tok + 1;
    if (!isdigit((unsigned char)*p))
        return false;
    int col = 0;
    while (isdigit((unsigned char)*p)) {
        col = col * 10 + (*p - '0');
        p++;
    }
    if (col < 1 || col > LOBBS_WALL_COLS)
        return false;
    colOut = col;
    if (endOut)
        *endOut = p;
    return true;
}

static bool parseWallToken(const char *tok, int &rowOut, int &colOut, char &chOut)
{
    if (!tok || !tok[0])
        return false;
    const char *coord = tok;
    if (tok[0] == '-') {
        coord = tok + 1;
        if (!coord[0])
            return false;
        const char *end = nullptr;
        if (!parseCoord(coord, rowOut, colOut, &end) || !end || *end != '\0')
            return false;
        chOut = ' ';
        return true;
    }
    const char *end = nullptr;
    if (!parseCoord(coord, rowOut, colOut, &end) || !end || end[1] != '\0')
        return false;
    chOut = *end;
    if ((unsigned char)chOut < 0x20 || (unsigned char)chOut > 0x7e)
        return false;
    return true;
}

const char *WallDal::applyPaintTokens(uint64_t userUuid, bool isSysop, const char *const *tokens, int count,
                                      uint32_t periodSeconds, uint32_t maxCellsPerCycle)
{
    if (count <= 0)
        return "No paint tokens.";
    CanvasState canvas = {};
    loadCanvas(canvas);
    for (int i = 0; i < count; i++) {
        int row = 0;
        int col = 0;
        char ch = 0;
        if (!parseWallToken(tokens[i], row, col, ch))
            return "Bad token.";
        canvas.cells[row * LOBBS_WALL_COLS + (col - 1)] = ch;
    }
    wallSealCells(canvas.cells);

    uint32_t cells = (uint32_t)count;
    if (!isSysop) {
        if (cells > maxCellsPerCycle)
            return "Too many cells.";
        uint32_t used = 0;
        lobbsQuotaUsed(lodb_, "wall_quota", userUuid, periodSeconds, &used, 1);
        if (used + cells > maxCellsPerCycle)
            return "Quota full.";
    }
    LoDbError err = saveCanvas(canvas);
    if (err != LODB_OK)
        return lobbsDbErrorText(err, "Save failed.");
    if (!isSysop && (err = lobbsQuotaAdd(lodb_, "wall_quota", userUuid, periodSeconds, &cells, 1)) != LODB_OK)
        return lobbsDbErrorText(err, "Quota save.");
    return nullptr;
}

