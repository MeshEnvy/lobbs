#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <lofs/LoFS.h>
#include <loscalar/LoScalar.h>
#include <map>
#include <string>
#include <vector>

/**
 * LoDB - Synchronous LoScalar Database
 *
 * Filesystem-backed records under `<root>/<name>` after `open(root)`. I/O goes through LoFS.
 */

#ifndef LODB_VERSION
#define LODB_VERSION "2.0.0"
#endif

/**
 * Max encoded record size (single knob): insert/update encode buffer and get() read cap.
 */
#ifndef LODB_MAX_RECORD_BYTES
#define LODB_MAX_RECORD_BYTES 1024
#endif
#ifndef LODB_FILE_IO_BUFFER_SIZE
#define LODB_FILE_IO_BUFFER_SIZE LODB_MAX_RECORD_BYTES
#endif
#ifndef LODB_MAX_RECORD_FILE_BYTES
#define LODB_MAX_RECORD_FILE_BYTES LODB_MAX_RECORD_BYTES
#endif

#ifndef LODB_LOG_DEBUG
#define LODB_LOG_DEBUG(...) ((void)0)
#endif
#ifndef LODB_LOG_INFO
#define LODB_LOG_INFO(...) ((void)0)
#endif
#ifndef LODB_LOG_WARN
#define LODB_LOG_WARN(...) ((void)0)
#endif
#ifndef LODB_LOG_ERROR
#define LODB_LOG_ERROR(...) ((void)0)
#endif

typedef uint64_t lodb_uuid_t;

#define LODB_UUID_FMT "%08x%08x"
#define LODB_UUID_ARGS(uuid) (uint32_t)((uuid) >> 32), (uint32_t)((uuid)&0xFFFFFFFF)

typedef enum {
    LODB_OK = 0,
    LODB_ERR_NOT_FOUND,
    LODB_ERR_IO,
    LODB_ERR_DECODE,
    LODB_ERR_ENCODE,
    LODB_ERR_INVALID,
    /** Write refused or cut short: the mount lacks room above its reserve (`LoFS::hasRoom`). */
    LODB_ERR_FULL
} LoDbError;

typedef std::function<bool(const LoScalar &)> LoDbFilter;
typedef std::function<int(const LoScalar &, const LoScalar &)> LoDbComparator;

/** LoScalar field numbers are 0..99 inclusive. */
static constexpr uint32_t LODB_F_MAX = 99;
/** App fields use 0 .. LODB_F_USER_LIMIT - 1. */
static constexpr uint32_t LODB_F_USER_LIMIT = 94;

static constexpr uint32_t LODB_F_ERROR = 94;
static constexpr uint32_t LODB_F_UPDATED = 95;
static constexpr uint32_t LODB_F_CREATED = 96;
static constexpr uint32_t LODB_F_DESCRIPTION = 97;
static constexpr uint32_t LODB_F_TITLE = 98;
static constexpr uint32_t LODB_F_ID = 99;

void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17]);
lodb_uuid_t lodb_new_uuid(const char *str, uint64_t salt);

uint32_t lodb_now_ms(void);
uint32_t lodb_now_unix(void);

class LoDb
{
  public:
    LoDb(const char *db_name);
    ~LoDb();

    bool isOpen() const { return opened_; }
    LoDbError open(const char *root);
    LoDbError registerTable(const char *table_name);
    LoDbError insert(const char *table_name, lodb_uuid_t uuid, const LoScalar &record);
    LoDbError get(const char *table_name, lodb_uuid_t uuid, LoScalar &record_out);
    LoDbError update(const char *table_name, lodb_uuid_t uuid, const LoScalar &record);
    LoDbError upsert(const char *table_name, lodb_uuid_t uuid, const LoScalar &record);
    LoDbError deleteRecord(const char *table_name, lodb_uuid_t uuid);
    std::vector<LoScalar> select(const char *table_name, LoDbFilter filter = LoDbFilter(),
                                 LoDbComparator comparator = LoDbComparator(), size_t limit = 0);
    int count(const char *table_name, LoDbFilter filter = LoDbFilter());

  private:
    struct TableMetadata {
        std::string table_name;
        char table_path[160];
    };

    std::string db_name;
    char db_path[128];
    bool opened_ = false;
    std::map<std::string, TableMetadata> tables;

    TableMetadata *getTable(const char *table_name);
    bool recordPath(const char *table_name, lodb_uuid_t uuid, char *out, size_t cap);
    static bool mkdirPathSegments(const char *path);
};
