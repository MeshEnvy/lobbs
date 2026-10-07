#include <lodb/LoDB.h>
#include "platforms/LoPlatform.h"

#undef LODB_LOG_DEBUG
#define LODB_LOG_DEBUG(...) LOBBS_LOG_DEBUG(__VA_ARGS__)
#undef LODB_LOG_INFO
#define LODB_LOG_INFO(...) LOBBS_LOG_INFO(__VA_ARGS__)
#undef LODB_LOG_WARN
#define LODB_LOG_WARN(...) LOBBS_LOG_WARN(__VA_ARGS__)
#undef LODB_LOG_ERROR
#define LODB_LOG_ERROR(...) LOBBS_LOG_ERROR(__VA_ARGS__)
#include <SHA256.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

#include "core/LoBBSStackGuard.h"

static_assert(LODB_FILE_IO_BUFFER_SIZE <= LODB_MAX_RECORD_FILE_BYTES,
              "encode buffer cannot exceed max on-disk record (get would reject writes)");

static constexpr size_t LODB_RECORD_PATH_BYTES = 192;

uint32_t lodb_now_ms(void)
{
    return lobbsPlatformMillis();
}

uint32_t lodb_now_unix(void)
{
    return lobbsPlatformUnixNow();
}

static void lodbStampInsert(lodb_uuid_t uuid, LoScalar &record)
{
    const uint32_t now = lodb_now_unix();
    record.setUint64(LODB_F_ID, uuid);
    if (!record.has(LODB_F_CREATED))
        record.setUint32(LODB_F_CREATED, now);
    if (!record.has(LODB_F_UPDATED))
        record.setUint32(LODB_F_UPDATED, now);
}

static void lodbStampUpdate(lodb_uuid_t uuid, LoScalar &record, const LoScalar *stored)
{
    const uint32_t now = lodb_now_unix();
    record.setUint64(LODB_F_ID, uuid);
    if (record.has(LODB_F_CREATED)) {
        // caller value kept
    } else if (stored && stored->has(LODB_F_CREATED)) {
        uint32_t created = 0;
        if (stored->getUint32(LODB_F_CREATED, created))
            record.setUint32(LODB_F_CREATED, created);
        else
            record.setUint32(LODB_F_CREATED, now);
    } else {
        record.setUint32(LODB_F_CREATED, now);
    }
    if (!record.has(LODB_F_UPDATED))
        record.setUint32(LODB_F_UPDATED, now);
}

void lodb_uuid_to_hex(lodb_uuid_t uuid, char hex_out[17])
{
    snprintf(hex_out, 17, LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
}

lodb_uuid_t lodb_new_uuid(const char *str, uint64_t salt)
{
    char generated_str[32];
    const char *input_str = str;

    if (str == nullptr) {
        uint32_t timestamp = lodb_now_ms();
        uint32_t random_val = lobbsPlatformRandom(0x7fffffff) ^ (lobbsPlatformRandom(0x7fffffff) << 1);
        snprintf(generated_str, sizeof(generated_str), "%u:%u", timestamp, random_val);
        input_str = generated_str;
    }

    SHA256 sha256;
    uint8_t hash[32];

    sha256.reset();
    sha256.update(input_str, strlen(input_str));

    uint8_t salt_bytes[8];
    memcpy(salt_bytes, &salt, 8);
    sha256.update(salt_bytes, 8);

    sha256.finalize(hash, 32);

    lodb_uuid_t uuid;
    memcpy(&uuid, hash, sizeof(lodb_uuid_t));
    return uuid;
}

LoDb::LoDb(const char *db_name) : db_name(db_name)
{
    db_path[0] = '\0';
}

bool LoDb::mkdirPathSegments(const char *path)
{
    if (!path || path[0] != '/')
        return false;
    char buf[128];
    size_t len = strlen(path);
    if (len >= sizeof(buf))
        return false;
    memcpy(buf, path, len + 1);

    for (size_t i = 1; i < len; i++) {
        if (buf[i] != '/')
            continue;
        buf[i] = '\0';
        if (!LoFS::mkdir(buf))
            LODB_LOG_DEBUG("mkdir segment may exist: %s", buf);
        buf[i] = '/';
    }
    if (!LoFS::mkdir(buf))
        LODB_LOG_DEBUG("Database directory may already exist: %s", buf);
    return true;
}

LoDbError LoDb::open(const char *root)
{
    if (!root || root[0] != '/')
        return LODB_ERR_INVALID;

    if (snprintf(db_path, sizeof(db_path), "%s/%s", root, db_name.c_str()) >= (int)sizeof(db_path))
        return LODB_ERR_INVALID;

    if (!mkdirPathSegments(db_path))
        return LODB_ERR_IO;

    for (auto &entry : tables) {
        if (snprintf(entry.second.table_path, sizeof(entry.second.table_path), "%s/%s", db_path, entry.first.c_str()) >=
            (int)sizeof(entry.second.table_path))
            return LODB_ERR_INVALID;
        if (!LoFS::mkdir(entry.second.table_path))
            LODB_LOG_DEBUG("Table directory may already exist: %s", entry.second.table_path);
    }

    opened_ = true;
    LODB_LOG_INFO("Opened LoDB database: %s", db_path);
    return LODB_OK;
}

LoDb::~LoDb() {}

/** Calls fn for each `<16 hex>.ls` record in `dir_path`. False when the path is not a directory. */
static bool lodbForEachRecord(const char *dir_path, const std::function<void(lodb_uuid_t)> &fn)
{
    LoFile dir = LoFS::open(dir_path, "r");
    if (!dir) {
        LODB_LOG_DEBUG("Table directory not found: %s", dir_path);
        return true;
    }
    if (!dir.isDirectory()) {
        LODB_LOG_ERROR("Table path is not a directory: %s", dir_path);
        dir.close();
        return false;
    }
    while (true) {
        LoFile file = dir.openNextFile();
        if (!file)
            break;
        bool isDir = file.isDirectory();
        std::string path = file.name();
        file.close();
        if (isDir)
            continue;
        size_t slash = path.rfind('/');
        const char *name = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
        uint32_t high = 0;
        uint32_t low = 0;
        if (strlen(name) != 19 || strcmp(name + 16, ".ls") != 0 || sscanf(name, "%08x%08x", &high, &low) != 2) {
            LODB_LOG_DEBUG("Skipped non-record file: %s", name);
            continue;
        }
        fn(((uint64_t)high << 32) | (uint64_t)low);
    }
    dir.close();
    return true;
}

bool LoDb::recordPath(const char *table_name, lodb_uuid_t uuid, char *out, size_t cap)
{
    TableMetadata *table = table_name ? getTable(table_name) : nullptr;
    if (!table)
        return false;
    char uuid_hex[17];
    lodb_uuid_to_hex(uuid, uuid_hex);
    return snprintf(out, cap, "%s/%s.ls", table->table_path, uuid_hex) < (int)cap;
}

LoDbError LoDb::registerTable(const char *table_name)
{
    if (!table_name) {
        return LODB_ERR_INVALID;
    }

    TableMetadata metadata;
    metadata.table_name = table_name;
    metadata.table_path[0] = '\0';

    if (opened_) {
        snprintf(metadata.table_path, sizeof(metadata.table_path), "%s/%s", db_path, table_name);
        if (!LoFS::mkdir(metadata.table_path))
            LODB_LOG_DEBUG("Table directory may already exist: %s", metadata.table_path);
    }

    tables[table_name] = metadata;
    LODB_LOG_INFO("Registered table: %s", table_name);
    return LODB_OK;
}

LoDb::TableMetadata *LoDb::getTable(const char *table_name)
{
    auto it = tables.find(table_name);
    if (it == tables.end()) {
        LODB_LOG_ERROR("Table not registered: %s", table_name);
        return nullptr;
    }
    return &it->second;
}

/** Writes to `<path>.w` then renames over `path`, so a failed write never truncates the record. */
static LoDbError writeRecordFile(const char *file_path, const LoScalar &record)
{
    std::string line;
    if (!record.encode(line, LODB_FILE_IO_BUFFER_SIZE)) {
        LODB_LOG_ERROR("Failed to encode LoScalar record");
        return LODB_ERR_ENCODE;
    }

    char tmp_path[LODB_RECORD_PATH_BYTES + 2];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.w", file_path) >= (int)sizeof(tmp_path)) {
        LODB_LOG_ERROR("Temp path too long: %s", file_path);
        return LODB_ERR_IO;
    }
    LoFS::remove(tmp_path);

    size_t encoded_size = line.size();
    if (!LoFS::hasRoom(tmp_path, (uint32_t)encoded_size)) {
        LODB_LOG_WARN("No room for record: %s", file_path);
        return LODB_ERR_FULL;
    }

    auto file = LoFS::open(tmp_path, "w");
    if (!file) {
        LODB_LOG_ERROR("Failed to open file for writing: %s", tmp_path);
        return LODB_ERR_IO;
    }

    size_t written = file.write((const uint8_t *)line.data(), encoded_size);
    file.flush();
    file.close();
    if (written != encoded_size) {
        LODB_LOG_ERROR("Failed to write file, wrote %u of %u bytes", (unsigned)written, (unsigned)encoded_size);
        LoFS::remove(tmp_path);
        return LoFS::hasRoom(tmp_path, (uint32_t)encoded_size) ? LODB_ERR_IO : LODB_ERR_FULL;
    }

    if (!LoFS::rename(tmp_path, file_path)) {
        LODB_LOG_ERROR("Failed to rename temp to: %s", file_path);
        LoFS::remove(tmp_path);
        return LODB_ERR_IO;
    }

    LODB_LOG_DEBUG("Wrote record to: %s (%u bytes)", file_path, (unsigned)encoded_size);
    return LODB_OK;
}

static LoDbError writeUpdatedRecord(const char *file_path, lodb_uuid_t uuid, const LoScalar &record, const LoScalar &stored)
{
    LoScalar stamped = record;
    lodbStampUpdate(uuid, stamped, &stored);
    LoDbError err = writeRecordFile(file_path, stamped);
    if (err == LODB_OK)
        LODB_LOG_INFO("Updated record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return err;
}

LoDbError LoDb::insert(const char *table_name, lodb_uuid_t uuid, const LoScalar &record)
{
    if (!opened_)
        return LODB_ERR_IO;
    char file_path[LODB_RECORD_PATH_BYTES];
    if (!recordPath(table_name, uuid, file_path, sizeof(file_path)))
        return LODB_ERR_INVALID;

    {
        auto existing = LoFS::open(file_path, "r");
        if (existing) {
            size_t existingSize = existing.size();
            existing.close();
            if (existingSize == 0) {
                LoFS::remove(file_path);
            } else {
                LODB_LOG_ERROR("UUID already exists: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
                return LODB_ERR_INVALID;
            }
        }
    }

    LoScalar stamped = record;
    lodbStampInsert(uuid, stamped);
    LoDbError err = writeRecordFile(file_path, stamped);
    if (err != LODB_OK)
        return err;

    LODB_LOG_INFO("Inserted record with custom UUID: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

LoDbError LoDb::get(const char *table_name, lodb_uuid_t uuid, LoScalar &record_out)
{
    if (!opened_)
        return LODB_ERR_IO;
    char file_path[LODB_RECORD_PATH_BYTES];
    if (!recordPath(table_name, uuid, file_path, sizeof(file_path)))
        return LODB_ERR_INVALID;

    auto file = LoFS::open(file_path, "r");
    if (!file) {
        LODB_LOG_DEBUG("Record not found: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }

    size_t total_size = file.size();
    if (total_size == 0) {
        file.close();
        LoFS::remove(file_path);
        LODB_LOG_WARN("Removed empty record file: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_NOT_FOUND;
    }
    if (total_size > LODB_MAX_RECORD_FILE_BYTES) {
        LODB_LOG_ERROR("Record file too large: %s (%u > %u)", file_path, (unsigned)total_size,
                       (unsigned)LODB_MAX_RECORD_FILE_BYTES);
        file.close();
        return LODB_ERR_IO;
    }

    std::unique_ptr<char[]> buffer(new (std::nothrow) char[total_size + 1]);
    if (!buffer) {
        LODB_LOG_ERROR("LoDB get: buffer alloc failed (%u bytes)", (unsigned)total_size);
        file.close();
        return LODB_ERR_IO;
    }

    size_t file_size = file.read((uint8_t *)buffer.get(), total_size);
    file.close();

    if (file_size == 0 || file_size != total_size) {
        LODB_LOG_ERROR("Record read bad size: %s (%u of %u)", file_path, (unsigned)file_size, (unsigned)total_size);
        return LODB_ERR_IO;
    }

    buffer[file_size] = '\0';
    while (file_size > 0 && (buffer[file_size - 1] == '\n' || buffer[file_size - 1] == '\r'))
        file_size--;

    record_out.clear();
    if (!record_out.decode(buffer.get(), file_size)) {
        LODB_LOG_ERROR("Failed to decode LoScalar from " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_ERR_DECODE;
    }

    LODB_LOG_DEBUG("Retrieved record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_OK;
}

LoDbError LoDb::update(const char *table_name, lodb_uuid_t uuid, const LoScalar &record)
{
    char file_path[LODB_RECORD_PATH_BYTES];
    if (!recordPath(table_name, uuid, file_path, sizeof(file_path)))
        return LODB_ERR_INVALID;
    LoScalar stored;
    LoDbError err = get(table_name, uuid, stored);
    if (err != LODB_OK)
        return err;
    return writeUpdatedRecord(file_path, uuid, record, stored);
}

LoDbError LoDb::upsert(const char *table_name, lodb_uuid_t uuid, const LoScalar &record)
{
    char file_path[LODB_RECORD_PATH_BYTES];
    if (!recordPath(table_name, uuid, file_path, sizeof(file_path)))
        return LODB_ERR_INVALID;
    LoScalar stored;
    LoDbError err = get(table_name, uuid, stored);
    if (err == LODB_OK)
        return writeUpdatedRecord(file_path, uuid, record, stored);
    if (err != LODB_ERR_NOT_FOUND)
        return err;
    return insert(table_name, uuid, record);
}

LoDbError LoDb::deleteRecord(const char *table_name, lodb_uuid_t uuid)
{
    if (!opened_)
        return LODB_ERR_IO;
    char file_path[LODB_RECORD_PATH_BYTES];
    if (!recordPath(table_name, uuid, file_path, sizeof(file_path)))
        return LODB_ERR_INVALID;

    if (!LoFS::exists(file_path))
        return LODB_ERR_NOT_FOUND;
    if (LoFS::remove(file_path)) {
        LODB_LOG_DEBUG("Deleted record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
        return LODB_OK;
    }
    LODB_LOG_WARN("Failed to delete record: " LODB_UUID_FMT, LODB_UUID_ARGS(uuid));
    return LODB_ERR_IO;
}

std::vector<LoScalar> LoDb::select(const char *table_name, LoDbFilter filter, LoDbComparator comparator, size_t limit)
{
    std::vector<LoScalar> results;
    if (!opened_)
        return results;

    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return results;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not found: %s", table_name);
        return results;
    }

    lodbForEachRecord(table->table_path, [&](lodb_uuid_t uuid) {
        LoScalar record;
        if (get(table_name, uuid, record) != LODB_OK) {
            LODB_LOG_WARN("Failed to read record " LODB_UUID_FMT " during select", LODB_UUID_ARGS(uuid));
            return;
        }
        if (filter && !filter(record))
            return;
        results.push_back(record);
    });

    LODB_LOG_INFO("Select from %s: %u records after filtering", table_name, (unsigned)results.size());

    if (comparator && !results.empty()) {
        std::sort(results.begin(), results.end(),
                  [&comparator](const LoScalar &a, const LoScalar &b) { return comparator(a, b) < 0; });
        LODB_LOG_DEBUG("Sorted %u records", (unsigned)results.size());
    }

    if (limit > 0 && results.size() > limit) {
        results.resize(limit);
        LODB_LOG_DEBUG("Limited results to %u records", (unsigned)limit);
    }

    LODB_LOG_INFO("Select from %s complete: %u records returned", table_name, (unsigned)results.size());
    return results;
}

int LoDb::count(const char *table_name, LoDbFilter filter)
{
    if (!opened_)
        return 0;
    if (!table_name) {
        LODB_LOG_ERROR("Invalid table_name");
        return -1;
    }

    TableMetadata *table = getTable(table_name);
    if (!table) {
        LODB_LOG_ERROR("Table not found: %s", table_name);
        return -1;
    }

    int cnt = 0;

    if (!filter) {
        if (!lodbForEachRecord(table->table_path, [&](lodb_uuid_t) { cnt++; }))
            return -1;
        LODB_LOG_DEBUG("Counted %d records in %s (no filter)", cnt, table_name);
        return cnt;
    }

    auto results = select(table_name, filter, LoDbComparator(), 0);
    cnt = (int)results.size();

    LODB_LOG_DEBUG("Counted %d records in %s (with filter)", cnt, table_name);
    return cnt;
}
