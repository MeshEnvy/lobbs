#pragma once

#include "lofs/lolog/LoLogConfig.h"
#include "lofs/volumes/LoFSRawDevice.h"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace lolog {

struct Loc {
    uint16_t sector;
    uint16_t off;
};

} // namespace lolog

class LoLog {
  public:
    explicit LoLog(LoFSRawDevice &device);

    bool mount();
    bool format();
    void maintain(uint32_t budgetMs);

    uint64_t totalBytes() const;
    uint64_t usedBytes() const;
    void spaceHint(uint32_t *blockOut, uint32_t *slackOut) const;

    bool exists(const char *relPath);
    bool isDirectory(const char *relPath);
    bool statPath(const char *relPath, uint32_t *sizeOut, bool *isDirOut);

    bool mkdir(const char *relPath);
    bool remove(const char *relPath);
    bool rename(const char *oldPath, const char *newPath);
    bool rmdir(const char *relPath, bool recursive);

    struct OpenHandle {
        bool isDirIter = false;
        uint32_t dirId = 0;
        size_t dirIdx = 0;
        uint32_t inodeId = 0;
        bool write = false;
        bool dirty = false;
        uint32_t pos = 0;
        std::vector<uint8_t> writeBuf;
        char nameBuf[64];
    };

    OpenHandle *open(const char *relPath, bool write, bool create);
    void closeHandle(OpenHandle *h);
    size_t readHandle(OpenHandle *h, uint8_t *buf, size_t len);
    size_t writeHandle(OpenHandle *h, const uint8_t *buf, size_t len);
    bool seekHandle(OpenHandle *h, uint32_t pos);
    uint32_t sizeHandle(OpenHandle *h) const;
    bool isDirHandle(OpenHandle *h) const;
    void flushHandle(OpenHandle *h);
    const char *nameHandle(OpenHandle *h) const;

    OpenHandle *openDir(const char *relPath);
    OpenHandle *openNextInDir(OpenHandle *dir);

    bool writeAtPath(const char *relPath, uint32_t offset, const uint8_t *data, size_t len);

  private:
    LoFSRawDevice &dev_;
    bool mounted_ = false;
    uint32_t epoch_ = 0;
    uint32_t nextSeq_ = 1;
    uint32_t nextDirId_ = 1;
    uint32_t nextInodeId_ = 1;
    uint32_t coveredSeq_ = 0;
    uint64_t liveBytes_ = 0;
    uint32_t lastActivityMs_ = 0;

    uint16_t activeSector_ = 0xFFFF;
    uint32_t activeOff_ = lolog::kHeaderSize;

    std::vector<uint8_t> sectorUsed_;
    std::vector<uint32_t> liveInSector_;
    std::vector<uint32_t> eraseMirror_;

    struct FileMeta {
        uint32_t dirId = 0;
        std::string name;
        bool isDir = false;
        uint32_t inodeId = 0;
        uint32_t dirIdSelf = 0;
        uint32_t parentDirId = 0;
        uint32_t size = 0;
        uint32_t mtime = 0;
        std::vector<uint8_t> data;
        lolog::Loc loc{};
    };

    std::vector<FileMeta> byInode_;
    /** dirId -> parentDirId (root parent is self). */
    std::vector<uint32_t> dirParent_;
    /** (dirId << 32 | hash) -> inode or dir id stored in name map via string key */
    struct NameKey {
        uint32_t dirId;
        std::string name;
        bool operator<(const NameKey &o) const;
    };
    struct NameVal {
        bool isDir;
        uint32_t id;
    };
    std::vector<std::pair<NameKey, NameVal>> nameIndex_;

    struct PendingEnt {
        uint8_t type;
        std::vector<uint8_t> payload;
    };
    std::vector<PendingEnt> pendingGroup_;

    std::vector<uint32_t> deadDirIds_;
    std::vector<uint32_t> indexRunSeqs_;
    uint32_t memtableBytes_ = 0;
    uint32_t lastFlushMs_ = 0;
    static constexpr uint32_t kIdleFlushMs = 30000;

    uint32_t crc32(const uint8_t *data, size_t len) const;
    bool readBytes(uint32_t addr, void *buf, size_t len);
    bool progBytes(uint32_t addr, const void *buf, size_t len);
    bool readSectorHeader(uint32_t sector, uint32_t &magic, uint32_t &epoch, uint32_t &seq, uint8_t &kind);
    bool writeSectorHeader(uint32_t sector, uint32_t seq, uint8_t kind);
    bool allocDataSector();
    bool appendRaw(const uint8_t *payload, size_t payloadLen, uint8_t type, uint8_t flags);
    void beginGroup();
    bool endGroup();
    void applyEntry(uint8_t type, const uint8_t *payload, size_t len);
    void replaySector(uint32_t sector);
    void rebuildIndexFromFlash();
    bool resolvePath(const char *relPath, uint32_t &dirIdOut, std::string &nameOut, bool createDirs);
    bool lookupName(uint32_t dirId, const char *name, NameVal &out) const;
    void upsertName(uint32_t dirId, const char *name, const NameVal &val);
    void eraseName(uint32_t dirId, const char *name);
    FileMeta *metaByInode(uint32_t inodeId);
    const FileMeta *metaByInode(uint32_t inodeId) const;
    bool commitFileMeta(FileMeta &meta);
    bool dirEmpty(uint32_t dirId) const;
    void tombDir(uint32_t dirId);
    bool hasRoomFor(uint32_t bytes) const;
    uint32_t freeSectors() const;
    void cleanOneSector();
    bool allocIndexSector(uint32_t &sectorOut, uint32_t runSeq);
    bool flushIndexRun();
    void mergeIndexRunsIfNeeded();
    bool loadNewestIndexRun();
    void flushIndexIfNeeded();
    bool copyLiveGroupsFromSector(uint32_t sector);
    uint32_t nowMs() const;
};
