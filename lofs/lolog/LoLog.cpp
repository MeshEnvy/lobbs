#include "LoLog.h"
#include "lofs/lolog/LoLogConfig.h"
#include <algorithm>
#include <cstring>
#include <string>

#if defined(LOBBS_PLATFORM_MESHTASTIC) || defined(LOBBS_PLATFORM_MESHCORE)
#include "platforms/LoPlatform.h"
#else
static uint32_t lologStubMs()
{
    static uint32_t t;
    return t++;
}
#endif

static uint32_t lologNowMs()
{
#if defined(LOBBS_PLATFORM_MESHTASTIC) || defined(LOBBS_PLATFORM_MESHCORE)
    return lobbsPlatformMillis();
#else
    return lologStubMs();
#endif
}

bool LoLog::NameKey::operator<(const NameKey &o) const
{
    if (dirId != o.dirId)
        return dirId < o.dirId;
    return name < o.name;
}

LoLog::LoLog(LoFSRawDevice &device) : dev_(device) {}

uint32_t LoLog::crc32(const uint8_t *data, size_t len) const
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0xEDB88320 & (~((crc & 1) - 1)));
    }
    return ~crc;
}

bool LoLog::readBytes(uint32_t addr, void *buf, size_t len)
{
    return dev_.read(addr, buf, len);
}

bool LoLog::progBytes(uint32_t addr, const void *buf, size_t len)
{
    if ((addr & 3) != 0 || (len & 3) != 0)
        return false;
    return dev_.prog(addr, buf, len);
}

bool LoLog::readSectorHeader(uint32_t sector, uint32_t &magic, uint32_t &epoch, uint32_t &seq, uint8_t &kind)
{
    uint8_t hdr[lolog::kHeaderSize];
    if (!readBytes(sector * lolog::kSectorSize, hdr, sizeof(hdr)))
        return false;
    magic = (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24);
    epoch = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    seq = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) | ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    kind = hdr[12];
    uint32_t hcrc = (uint32_t)hdr[13] | ((uint32_t)hdr[14] << 8) | ((uint32_t)hdr[15] << 16);
    uint32_t calc = crc32(hdr, 13) & 0xFFFFFF;
    if (magic != lolog::kMagicUsed || calc != hcrc)
        return false;
    return true;
}

bool LoLog::writeSectorHeader(uint32_t sector, uint32_t seq, uint8_t kind)
{
    uint8_t hdr[lolog::kHeaderSize];
    memset(hdr, 0xFF, sizeof(hdr));
    hdr[0] = (uint8_t)(lolog::kMagicUsed);
    hdr[1] = (uint8_t)(lolog::kMagicUsed >> 8);
    hdr[2] = (uint8_t)(lolog::kMagicUsed >> 16);
    hdr[3] = (uint8_t)(lolog::kMagicUsed >> 24);
    hdr[4] = (uint8_t)(epoch_);
    hdr[5] = (uint8_t)(epoch_ >> 8);
    hdr[6] = (uint8_t)(epoch_ >> 16);
    hdr[7] = (uint8_t)(epoch_ >> 24);
    hdr[8] = (uint8_t)(seq);
    hdr[9] = (uint8_t)(seq >> 8);
    hdr[10] = (uint8_t)(seq >> 16);
    hdr[11] = (uint8_t)(seq >> 24);
    hdr[12] = kind;
    uint32_t hcrc = crc32(hdr, 13) & 0xFFFFFF;
    hdr[13] = (uint8_t)hcrc;
    hdr[14] = (uint8_t)(hcrc >> 8);
    hdr[15] = (uint8_t)(hcrc >> 16);
    return progBytes(sector * lolog::kSectorSize, hdr, sizeof(hdr));
}

bool LoLog::allocDataSector()
{
    const uint32_t n = dev_.sectorCount();
    if (n == 0)
        return false;
    static uint32_t rr = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t s = (rr + i) % n;
        if (s >= sectorUsed_.size() || sectorUsed_[s])
            continue;
        if (!dev_.eraseSector(s))
            continue;
        sectorUsed_[s] = 1;
        liveInSector_[s] = lolog::kHeaderSize;
        rr = (s + 1) % n;
        activeSector_ = (uint16_t)s;
        activeOff_ = lolog::kHeaderSize;
        if (!writeSectorHeader(s, nextSeq_++, lolog::kSegData))
            return false;
        return true;
    }
    return false;
}

bool LoLog::appendRaw(const uint8_t *payload, size_t payloadLen, uint8_t type, uint8_t flags)
{
    const uint32_t entLen = (uint32_t)(8 + payloadLen);
    const uint32_t padded = (entLen + 3) & ~3u;
    if (activeSector_ == 0xFFFF || activeOff_ + padded > lolog::kSectorSize) {
        if (!allocDataSector())
            return false;
    }
    if (!hasRoomFor(padded + lolog::kHeadroomSectors * lolog::kSectorSize))
        return false;

    uint8_t stack[8 + lolog::kInlineMax];
    if (payloadLen > lolog::kInlineMax)
        return false;
    stack[0] = (uint8_t)(entLen - 8);
    stack[1] = (uint8_t)((entLen - 8) >> 8);
    stack[2] = type;
    stack[3] = flags;
    memcpy(stack + 4, payload, payloadLen);
    uint32_t c = crc32(stack + 4, payloadLen);
    memcpy(stack + 4 + payloadLen, &c, 4);
    size_t pad = padded - entLen;
    if (pad)
        memset(stack + entLen, 0xFF, pad);

    uint32_t addr = activeSector_ * lolog::kSectorSize + activeOff_;
    if (!progBytes(addr, stack, padded))
        return false;
    activeOff_ += padded;
    liveInSector_[activeSector_] = activeOff_;
    liveBytes_ += padded;
    lastActivityMs_ = nowMs();
    return true;
}

void LoLog::beginGroup()
{
    pendingGroup_.clear();
}

bool LoLog::endGroup()
{
    for (size_t i = 0; i < pendingGroup_.size(); i++) {
        uint8_t fl = (i + 1 == pendingGroup_.size()) ? lolog::kFlagGroupEnd : 0;
        if (!appendRaw(pendingGroup_[i].payload.data(), pendingGroup_[i].payload.size(), pendingGroup_[i].type, fl))
            return false;
    }
    pendingGroup_.clear();
    return true;
}

void LoLog::applyEntry(uint8_t type, const uint8_t *payload, size_t len)
{
    if (type == lolog::kEntDir && len >= 8) {
        uint32_t parent = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) |
                          ((uint32_t)payload[3] << 24);
        uint32_t dirId = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) | ((uint32_t)payload[6] << 16) |
                         ((uint32_t)payload[7] << 24);
        const char *name = (const char *)(payload + 8);
        if (dirId >= dirParent_.size())
            dirParent_.resize(dirId + 1, lolog::kRootDirId);
        dirParent_[dirId] = parent;
        upsertName(parent, name, NameVal{true, dirId});
    } else if (type == lolog::kEntFile && len >= 16) {
        uint32_t dirId = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) |
                         ((uint32_t)payload[3] << 24);
        uint32_t inode = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) | ((uint32_t)payload[6] << 16) |
                          ((uint32_t)payload[7] << 24);
        uint32_t size = (uint32_t)payload[8] | ((uint32_t)payload[9] << 8) | ((uint32_t)payload[10] << 16) |
                        ((uint32_t)payload[11] << 24);
        uint32_t mtime = (uint32_t)payload[12] | ((uint32_t)payload[13] << 8) | ((uint32_t)payload[14] << 16) |
                         ((uint32_t)payload[15] << 24);
        const char *name = (const char *)(payload + 16);
        size_t nameLen = strnlen(name, len - 16);
        size_t inlineOff = 16 + nameLen + 1;
        FileMeta meta;
        meta.dirId = dirId;
        meta.name.assign(name, nameLen);
        meta.isDir = false;
        meta.inodeId = inode;
        meta.size = size;
        meta.mtime = mtime;
        if (inlineOff < len)
            meta.data.assign(payload + inlineOff, payload + len);
        if (inode >= byInode_.size())
            byInode_.resize(inode + 1);
        byInode_[inode] = meta;
        upsertName(dirId, meta.name.c_str(), NameVal{false, inode});
    } else if (type == lolog::kEntExtent && len >= 8) {
        uint32_t inode = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) |
                         ((uint32_t)payload[3] << 24);
        uint32_t offset = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) | ((uint32_t)payload[6] << 16) |
                          ((uint32_t)payload[7] << 24);
        const uint8_t *data = payload + 8;
        size_t dlen = len - 8;
        FileMeta *m = metaByInode(inode);
        if (!m)
            return;
        if (m->data.size() < offset + dlen)
            m->data.resize(offset + dlen);
        memcpy(m->data.data() + offset, data, dlen);
        if (offset + dlen > m->size)
            m->size = (uint32_t)(offset + dlen);
    } else if (type == lolog::kEntTomb && len >= 4) {
        uint32_t dirId = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8) | ((uint32_t)payload[2] << 16) |
                         ((uint32_t)payload[3] << 24);
        if (len == 4) {
            deadDirIds_.push_back(dirId);
            tombDir(dirId);
        } else {
            const char *name = (const char *)(payload + 4);
            eraseName(dirId, name);
        }
    }
}

void LoLog::replaySector(uint32_t sector)
{
    uint32_t magic, epoch, seq;
    uint8_t kind = 0;
    if (!readSectorHeader(sector, magic, epoch, seq, kind) || kind != lolog::kSegData)
        return;
    if (epoch != epoch_)
        return;
    if (seq <= coveredSeq_)
        return;

    uint32_t off = lolog::kHeaderSize;
    std::vector<PendingEnt> group;
    while (off + 8 <= lolog::kSectorSize) {
        uint8_t hdr[8];
        if (!readBytes(sector * lolog::kSectorSize + off, hdr, 8))
            break;
        uint16_t plen = (uint16_t)hdr[0] | ((uint16_t)hdr[1] << 8);
        if (plen == lolog::kEntryEnd)
            break;
        uint8_t type = hdr[2];
        uint8_t flags = hdr[3];
        uint32_t entLen = 8 + plen;
        uint32_t padded = (entLen + 3) & ~3u;
        if (off + padded > lolog::kSectorSize)
            break;
        std::vector<uint8_t> payload(plen);
        if (plen > 0 && !readBytes(sector * lolog::kSectorSize + off + 4, payload.data(), plen))
            break;
        uint32_t stored;
        if (!readBytes(sector * lolog::kSectorSize + off + 4 + plen, &stored, 4))
            break;
        if (crc32(payload.data(), plen) != stored)
            break;
        PendingEnt pe{type, std::move(payload)};
        group.push_back(std::move(pe));
        off += padded;
        if (flags & lolog::kFlagGroupEnd) {
            for (auto &e : group)
                applyEntry(e.type, e.payload.data(), e.payload.size());
            group.clear();
        }
    }
}

void LoLog::rebuildIndexFromFlash()
{
    nameIndex_.clear();
    byInode_.clear();
    dirParent_.clear();
    dirParent_.resize(1);
    dirParent_[0] = 0;
    sectorUsed_.assign(dev_.sectorCount(), 0);
    liveInSector_.assign(dev_.sectorCount(), 0);
    liveBytes_ = 0;
    epoch_ = 0;
    coveredSeq_ = 0;
    activeSector_ = 0xFFFF;

    uint32_t maxEpoch = 0;
    for (uint32_t s = 0; s < dev_.sectorCount(); s++) {
        uint32_t magic, ep, seq;
        uint8_t kind = 0;
        if (!readSectorHeader(s, magic, ep, seq, kind))
            continue;
        if (ep > maxEpoch)
            maxEpoch = ep;
    }
    if (maxEpoch == 0) {
        uint8_t probe;
        if (readBytes(0, &probe, 1) && probe == 0xFF)
            return;
    }
    epoch_ = maxEpoch;
    indexRunSeqs_.clear();

    for (uint32_t s = 0; s < dev_.sectorCount(); s++) {
        uint32_t magic, ep, seq;
        uint8_t kind = 0;
        if (!readSectorHeader(s, magic, ep, seq, kind))
            continue;
        if (ep != epoch_)
            continue;
        sectorUsed_[s] = 1;
        uint32_t end = lolog::kSectorSize;
        uint8_t scan[8];
        uint32_t o = lolog::kHeaderSize;
        while (o + 8 <= lolog::kSectorSize) {
            if (!readBytes(s * lolog::kSectorSize + o, scan, 8))
                break;
            uint16_t plen = (uint16_t)scan[0] | ((uint16_t)scan[1] << 8);
            if (plen == lolog::kEntryEnd) {
                end = o;
                break;
            }
            uint32_t padded = (8 + plen + 3) & ~3u;
            if (o + padded > lolog::kSectorSize)
                break;
            o += padded;
        }
        liveInSector_[s] = end;
        liveBytes_ += end;
        if (kind == lolog::kSegIndex)
            indexRunSeqs_.push_back(seq);
    }

    const bool hadIndex = loadNewestIndexRun();
    const uint32_t replayFrom = coveredSeq_;
    for (uint32_t s = 0; s < dev_.sectorCount(); s++) {
        uint32_t magic, ep, seq;
        uint8_t kind = 0;
        if (!readSectorHeader(s, magic, ep, seq, kind))
            continue;
        if (ep != epoch_ || kind != lolog::kSegData)
            continue;
        if (hadIndex && seq <= replayFrom)
            continue;
        replaySector(s);
    }

    for (uint32_t s = 0; s < dev_.sectorCount(); s++) {
        uint32_t magic, ep, seq;
        uint8_t kind = 0;
        if (readSectorHeader(s, magic, ep, seq, kind) && kind == lolog::kSegData && ep == epoch_) {
            if (activeSector_ == 0xFFFF || seq >= nextSeq_) {
                activeSector_ = (uint16_t)s;
                activeOff_ = liveInSector_[s];
                nextSeq_ = seq + 1;
            }
        }
    }
}

bool LoLog::mount()
{
    if (!dev_.begin())
        return false;
    mounted_ = true;
    rebuildIndexFromFlash();
    nextDirId_ = std::max(nextDirId_, (uint32_t)dirParent_.size());
    for (const auto &m : byInode_)
        if (m.inodeId >= nextInodeId_)
            nextInodeId_ = m.inodeId + 1;
    return true;
}

bool LoLog::format()
{
    if (!dev_.begin())
        return false;
    for (uint32_t s = 0; s < dev_.sectorCount(); s++)
        dev_.eraseSector(s);
    epoch_ = (uint32_t)lologNowMs() | 1;
    nextSeq_ = 1;
    nextDirId_ = 1;
    nextInodeId_ = 1;
    coveredSeq_ = 0;
    nameIndex_.clear();
    byInode_.clear();
    dirParent_.clear();
    dirParent_.resize(1);
    dirParent_[0] = 0;
    sectorUsed_.assign(dev_.sectorCount(), 0);
    liveInSector_.assign(dev_.sectorCount(), 0);
    liveBytes_ = 0;
    activeSector_ = 0xFFFF;
    mounted_ = true;
    return allocDataSector();
}

uint32_t LoLog::nowMs() const
{
    return lologNowMs();
}

uint64_t LoLog::totalBytes() const
{
    return (uint64_t)dev_.sectorCount() * dev_.sectorSize();
}

uint64_t LoLog::usedBytes() const
{
    return liveBytes_;
}

void LoLog::spaceHint(uint32_t *blockOut, uint32_t *slackOut) const
{
    if (blockOut)
        *blockOut = 256;
    if (slackOut)
        *slackOut = (lolog::kHeadroomSectors + 1) * lolog::kSectorSize;
}

uint32_t LoLog::freeSectors() const
{
    uint32_t n = 0;
    for (uint32_t s = 0; s < sectorUsed_.size(); s++)
        if (!sectorUsed_[s])
            n++;
    return n;
}

bool LoLog::hasRoomFor(uint32_t bytes) const
{
    const uint64_t reserve = (uint64_t)(lolog::kHeadroomSectors + 1) * lolog::kSectorSize;
    const uint64_t cap = totalBytes();
    return liveBytes_ + bytes + reserve <= cap;
}

bool LoLog::lookupName(uint32_t dirId, const char *name, NameVal &out) const
{
    NameKey k{dirId, name ? name : ""};
    for (const auto &p : nameIndex_) {
        if (p.first.dirId == k.dirId && p.first.name == k.name) {
            out = p.second;
            return true;
        }
    }
    return false;
}

void LoLog::upsertName(uint32_t dirId, const char *name, const NameVal &val)
{
    NameKey k{dirId, name ? name : ""};
    for (auto &p : nameIndex_) {
        if (p.first.dirId == k.dirId && p.first.name == k.name) {
            p.second = val;
            return;
        }
    }
    nameIndex_.push_back({k, val});
}

void LoLog::eraseName(uint32_t dirId, const char *name)
{
    NameKey k{dirId, name ? name : ""};
    nameIndex_.erase(
        std::remove_if(nameIndex_.begin(), nameIndex_.end(),
                       [&](const std::pair<NameKey, NameVal> &p) {
                           return p.first.dirId == k.dirId && p.first.name == k.name;
                       }),
        nameIndex_.end());
}

LoLog::FileMeta *LoLog::metaByInode(uint32_t inodeId)
{
    return inodeId < byInode_.size() ? &byInode_[inodeId] : nullptr;
}

const LoLog::FileMeta *LoLog::metaByInode(uint32_t inodeId) const
{
    return inodeId < byInode_.size() ? &byInode_[inodeId] : nullptr;
}

void LoLog::tombDir(uint32_t dirId)
{
    nameIndex_.erase(
        std::remove_if(nameIndex_.begin(), nameIndex_.end(),
                       [&](const std::pair<NameKey, NameVal> &p) {
                           return p.second.isDir && p.second.id == dirId;
                       }),
        nameIndex_.end());
}

bool LoLog::dirEmpty(uint32_t dirId) const
{
    for (const auto &p : nameIndex_) {
        if (p.first.dirId == dirId)
            return false;
    }
    return true;
}

static void splitPath(const char *rel, std::vector<std::string> &parts)
{
    parts.clear();
    if (!rel)
        return;
    while (*rel == '/')
        rel++;
    std::string cur;
    for (; *rel; rel++) {
        if (*rel == '/') {
            if (!cur.empty()) {
                parts.push_back(cur);
                cur.clear();
            }
        } else
            cur += *rel;
    }
    if (!cur.empty())
        parts.push_back(cur);
}

bool LoLog::resolvePath(const char *relPath, uint32_t &dirIdOut, std::string &nameOut, bool createDirs)
{
    std::vector<std::string> parts;
    splitPath(relPath, parts);
    if (parts.empty())
        return false;
    uint32_t dirId = lolog::kRootDirId;
    for (size_t i = 0; i + 1 < parts.size(); i++) {
        NameVal v;
        if (!lookupName(dirId, parts[i].c_str(), v) || !v.isDir)
            return false;
        dirId = v.id;
    }
    nameOut = parts.back();
    dirIdOut = dirId;
    if (createDirs) {
        (void)createDirs;
    }
    return true;
}

bool LoLog::exists(const char *relPath)
{
    uint32_t dirId;
    std::string name;
    if (!resolvePath(relPath, dirId, name, false))
        return strcmp(relPath, "/") == 0 || (relPath && relPath[0] == '\0');
    NameVal v;
    return lookupName(dirId, name.c_str(), v);
}

bool LoLog::isDirectory(const char *relPath)
{
    uint32_t dirId;
    std::string name;
    if (!resolvePath(relPath, dirId, name, false))
        return relPath && (strcmp(relPath, "/") == 0 || relPath[0] == '\0');
    NameVal v;
    if (!lookupName(dirId, name.c_str(), v))
        return false;
    return v.isDir;
}

bool LoLog::statPath(const char *relPath, uint32_t *sizeOut, bool *isDirOut)
{
    if (sizeOut)
        *sizeOut = 0;
    if (isDirOut)
        *isDirOut = false;
    if (relPath && (strcmp(relPath, "/") == 0 || relPath[0] == '\0')) {
        if (isDirOut)
            *isDirOut = true;
        return true;
    }
    uint32_t dirId;
    std::string name;
    if (!resolvePath(relPath, dirId, name, false))
        return false;
    NameVal v;
    if (!lookupName(dirId, name.c_str(), v))
        return false;
    if (isDirOut)
        *isDirOut = v.isDir;
    if (!v.isDir && sizeOut) {
        const FileMeta *m = metaByInode(v.id);
        if (m)
            *sizeOut = m->size;
    }
    return true;
}

bool LoLog::mkdir(const char *relPath)
{
    std::vector<std::string> parts;
    splitPath(relPath, parts);
    if (parts.empty())
        return false;
    uint32_t dirId = lolog::kRootDirId;
    for (size_t i = 0; i + 1 < parts.size(); i++) {
        NameVal v;
        if (!lookupName(dirId, parts[i].c_str(), v)) {
            std::string sub;
            for (size_t j = 0; j <= i; j++) {
                if (j)
                    sub += '/';
                sub += parts[j];
            }
            if (!mkdir(sub.c_str()))
                return false;
            if (!lookupName(dirId, parts[i].c_str(), v) || !v.isDir)
                return false;
        }
        dirId = v.id;
    }
    uint32_t parentDir = dirId;
    std::string name = parts.back();
    NameVal existing;
    if (lookupName(parentDir, name.c_str(), existing))
        return false;
    uint32_t newDir = nextDirId_++;
    if (newDir >= dirParent_.size())
        dirParent_.resize(newDir + 1, lolog::kRootDirId);
    dirParent_[newDir] = parentDir;
    beginGroup();
    uint8_t pl[8 + 64];
    pl[0] = (uint8_t)parentDir;
    pl[1] = (uint8_t)(parentDir >> 8);
    pl[2] = (uint8_t)(parentDir >> 16);
    pl[3] = (uint8_t)(parentDir >> 24);
    pl[4] = (uint8_t)newDir;
    pl[5] = (uint8_t)(newDir >> 8);
    pl[6] = (uint8_t)(newDir >> 16);
    pl[7] = (uint8_t)(newDir >> 24);
    strncpy((char *)(pl + 8), name.c_str(), 63);
    pendingGroup_.push_back({lolog::kEntDir, std::vector<uint8_t>(pl, pl + 8 + strlen((char *)(pl + 8)) + 1)});
    if (!endGroup())
        return false;
    upsertName(parentDir, name.c_str(), NameVal{true, newDir});
    return true;
}

bool LoLog::remove(const char *relPath)
{
    uint32_t dirId;
    std::string name;
    if (!resolvePath(relPath, dirId, name, false))
        return false;
    NameVal v;
    if (!lookupName(dirId, name.c_str(), v) || v.isDir)
        return false;
    beginGroup();
    uint8_t pl[4 + 64];
    pl[0] = (uint8_t)dirId;
    pl[1] = (uint8_t)(dirId >> 8);
    pl[2] = (uint8_t)(dirId >> 16);
    pl[3] = (uint8_t)(dirId >> 24);
    strncpy((char *)(pl + 4), name.c_str(), 63);
    pendingGroup_.push_back(
        {lolog::kEntTomb, std::vector<uint8_t>(pl, pl + 4 + strlen((char *)(pl + 4)) + 1)});
    if (!endGroup())
        return false;
    eraseName(dirId, name.c_str());
    return true;
}

bool LoLog::rename(const char *oldPath, const char *newPath)
{
    uint32_t oDir, nDir;
    std::string oName, nName;
    if (!resolvePath(oldPath, oDir, oName, false) || !resolvePath(newPath, nDir, nName, false))
        return false;
    NameVal v;
    if (!lookupName(oDir, oName.c_str(), v))
        return false;
    if (lookupName(nDir, nName.c_str(), v))
        return false;
    if (v.isDir) {
        if (!mkdir(newPath))
            return false;
        return rmdir(oldPath, true);
    }
    FileMeta *m = metaByInode(v.id);
    if (!m)
        return false;
    FileMeta copy = *m;
    copy.dirId = nDir;
    copy.name = nName;
    if (!commitFileMeta(copy))
        return false;
    remove(oldPath);
    return true;
}

bool LoLog::rmdir(const char *relPath, bool recursive)
{
    uint32_t dirId;
    std::string name;
    if (!resolvePath(relPath, dirId, name, false))
        return false;
    NameVal v;
    if (!lookupName(dirId, name.c_str(), v) || !v.isDir)
        return false;
    if (!recursive && !dirEmpty(v.id))
        return false;
    beginGroup();
    uint8_t pl[4];
    pl[0] = (uint8_t)v.id;
    pl[1] = (uint8_t)(v.id >> 8);
    pl[2] = (uint8_t)(v.id >> 16);
    pl[3] = (uint8_t)(v.id >> 24);
    pendingGroup_.push_back({lolog::kEntTomb, std::vector<uint8_t>(pl, pl + 4)});
    if (!endGroup())
        return false;
    tombDir(v.id);
    eraseName(dirId, name.c_str());
    return true;
}

static uint32_t paddedEntrySize(size_t payloadLen)
{
    const uint32_t entLen = (uint32_t)(8 + payloadLen);
    return (entLen + 3u) & ~3u;
}

bool LoLog::commitFileMeta(FileMeta &meta)
{
    const bool large = meta.data.size() > lolog::kInlineMax;
    const size_t inlineBytes = large ? 0 : meta.data.size();
    uint32_t groupNeed = paddedEntrySize(16 + meta.name.size() + 1 + inlineBytes);
    if (large) {
        size_t off = 0;
        while (off < meta.data.size()) {
            const size_t remain = meta.data.size() - off;
            const size_t chunk = remain < 512 ? remain : 512;
            groupNeed += paddedEntrySize(8 + chunk);
            off += chunk;
        }
    }
    if (!hasRoomFor(groupNeed))
        return false;
    if (meta.inodeId == 0) {
        meta.inodeId = nextInodeId_++;
        if (meta.inodeId >= byInode_.size())
            byInode_.resize(meta.inodeId + 1);
    }
    meta.size = (uint32_t)meta.data.size();
    meta.mtime = nowMs();
    beginGroup();
    std::vector<uint8_t> pl(16 + meta.name.size() + 1 + inlineBytes);
    pl[0] = (uint8_t)meta.dirId;
    pl[1] = (uint8_t)(meta.dirId >> 8);
    pl[2] = (uint8_t)(meta.dirId >> 16);
    pl[3] = (uint8_t)(meta.dirId >> 24);
    pl[4] = (uint8_t)meta.inodeId;
    pl[5] = (uint8_t)(meta.inodeId >> 8);
    pl[6] = (uint8_t)(meta.inodeId >> 16);
    pl[7] = (uint8_t)(meta.inodeId >> 24);
    pl[8] = (uint8_t)meta.size;
    pl[9] = (uint8_t)(meta.size >> 8);
    pl[10] = (uint8_t)(meta.size >> 16);
    pl[11] = (uint8_t)(meta.size >> 24);
    pl[12] = (uint8_t)meta.mtime;
    pl[13] = (uint8_t)(meta.mtime >> 8);
    pl[14] = (uint8_t)(meta.mtime >> 16);
    pl[15] = (uint8_t)(meta.mtime >> 24);
    memcpy(pl.data() + 16, meta.name.c_str(), meta.name.size() + 1);
    if (inlineBytes)
        memcpy(pl.data() + 16 + meta.name.size() + 1, meta.data.data(), inlineBytes);
    pendingGroup_.push_back({lolog::kEntFile, std::move(pl)});
    if (large) {
        size_t off = 0;
        while (off < meta.data.size()) {
            const size_t remain = meta.data.size() - off;
            size_t chunk = remain < 512 ? remain : 512;
            std::vector<uint8_t> ep(8 + chunk);
            ep[0] = (uint8_t)meta.inodeId;
            ep[1] = (uint8_t)(meta.inodeId >> 8);
            ep[2] = (uint8_t)(meta.inodeId >> 16);
            ep[3] = (uint8_t)(meta.inodeId >> 24);
            ep[4] = (uint8_t)off;
            ep[5] = (uint8_t)(off >> 8);
            ep[6] = (uint8_t)(off >> 16);
            ep[7] = (uint8_t)(off >> 24);
            memcpy(ep.data() + 8, meta.data.data() + off, chunk);
            pendingGroup_.push_back({lolog::kEntExtent, std::move(ep)});
            off += chunk;
        }
    }
    if (!endGroup())
        return false;
    byInode_[meta.inodeId] = meta;
    upsertName(meta.dirId, meta.name.c_str(), NameVal{false, meta.inodeId});
    return true;
}

LoLog::OpenHandle *LoLog::open(const char *relPath, bool write, bool create)
{
    uint32_t dirId;
    std::string name;
    if (!resolvePath(relPath, dirId, name, false)) {
        if (!create || !relPath)
            return nullptr;
        if (!resolvePath(relPath, dirId, name, false))
            return nullptr;
    }
    NameVal v;
    if (!lookupName(dirId, name.c_str(), v)) {
        if (!write || !create)
            return nullptr;
        FileMeta meta;
        meta.dirId = dirId;
        meta.name = name;
        meta.data.clear();
        if (!commitFileMeta(meta))
            return nullptr;
        v = NameVal{false, meta.inodeId};
    }
    if (v.isDir)
        return nullptr;
    auto *h = new OpenHandle();
    h->inodeId = v.id;
    h->write = write;
    h->pos = 0;
    FileMeta *m = metaByInode(v.id);
    if (m && write)
        h->writeBuf = m->data;
    strncpy(h->nameBuf, name.c_str(), sizeof(h->nameBuf) - 1);
    return h;
}

void LoLog::closeHandle(OpenHandle *h)
{
    if (!h)
        return;
    if (h->write && h->dirty) {
        FileMeta *m = metaByInode(h->inodeId);
        if (m) {
            m->data = h->writeBuf;
            commitFileMeta(*m);
        }
    }
    delete h;
}

size_t LoLog::readHandle(OpenHandle *h, uint8_t *buf, size_t len)
{
    if (!h || h->isDirIter || !buf)
        return 0;
    const FileMeta *m = metaByInode(h->inodeId);
    if (!m)
        return 0;
    if (h->pos >= m->data.size())
        return 0;
    const size_t avail = m->data.size() - (size_t)h->pos;
    size_t n = len < avail ? len : avail;
    memcpy(buf, m->data.data() + h->pos, n);
    h->pos += (uint32_t)n;
    return n;
}

size_t LoLog::writeHandle(OpenHandle *h, const uint8_t *buf, size_t len)
{
    if (!h || !h->write || !buf)
        return 0;
    static constexpr size_t kWriteBufMax = 65536;
    if (h->pos + len > kWriteBufMax)
        len = kWriteBufMax - h->pos;
    if (h->writeBuf.size() < h->pos + len)
        h->writeBuf.resize(h->pos + len);
    memcpy(h->writeBuf.data() + h->pos, buf, len);
    h->pos += (uint32_t)len;
    h->dirty = true;
    return len;
}

bool LoLog::seekHandle(OpenHandle *h, uint32_t pos)
{
    if (!h || h->isDirIter)
        return false;
    h->pos = pos;
    return true;
}

uint32_t LoLog::sizeHandle(OpenHandle *h) const
{
    if (!h || h->isDirIter)
        return 0;
    const FileMeta *m = metaByInode(h->inodeId);
    return m ? m->size : 0;
}

bool LoLog::isDirHandle(OpenHandle *h) const
{
    return h && h->isDirIter;
}

void LoLog::flushHandle(OpenHandle *h)
{
    if (!h || !h->write || !h->dirty)
        return;
    FileMeta *m = metaByInode(h->inodeId);
    if (m) {
        m->data = h->writeBuf;
        commitFileMeta(*m);
        h->dirty = false;
    }
}

const char *LoLog::nameHandle(OpenHandle *h) const
{
    return h ? h->nameBuf : "";
}

LoLog::OpenHandle *LoLog::openDir(const char *relPath)
{
    uint32_t dirId = lolog::kRootDirId;
    std::vector<std::string> parts;
    splitPath(relPath, parts);
    for (const auto &p : parts) {
        NameVal v;
        if (!lookupName(dirId, p.c_str(), v) || !v.isDir)
            return nullptr;
        dirId = v.id;
    }
    auto *h = new OpenHandle();
    h->isDirIter = true;
    h->dirId = dirId;
    h->dirIdx = 0;
    return h;
}

LoLog::OpenHandle *LoLog::openNextInDir(OpenHandle *dir)
{
    if (!dir || !dir->isDirIter)
        return nullptr;
    while (dir->dirIdx < nameIndex_.size()) {
        const auto &p = nameIndex_[dir->dirIdx++];
        if (p.first.dirId != dir->dirId)
            continue;
        auto *f = new OpenHandle();
        f->isDirIter = false;
        f->inodeId = p.second.isDir ? 0 : p.second.id;
        if (p.second.isDir)
            f->isDirIter = true;
        strncpy(f->nameBuf, p.first.name.c_str(), sizeof(f->nameBuf) - 1);
        if (p.second.isDir) {
            f->dirId = p.second.id;
            f->isDirIter = true;
        }
        return f;
    }
    return nullptr;
}

bool LoLog::writeAtPath(const char *relPath, uint32_t offset, const uint8_t *data, size_t len)
{
    OpenHandle *h = open(relPath, true, true);
    if (!h)
        return false;
    if (offset > 0)
        seekHandle(h, offset);
    size_t w = writeHandle(h, data, len);
    flushHandle(h);
    closeHandle(h);
    return w == len;
}

static constexpr uint32_t kIndexBodyTag = 0x49524958; // IRIX

bool LoLog::allocIndexSector(uint32_t &sectorOut, uint32_t runSeq)
{
    const uint32_t n = dev_.sectorCount();
    if (n == 0)
        return false;
    static uint32_t rrIdx = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t s = (rrIdx + i) % n;
        if (sectorUsed_[s])
            continue;
        uint32_t ec = dev_.eraseCount(s);
        (void)ec;
        if (!dev_.eraseSector(s))
            continue;
        sectorUsed_[s] = 1;
        liveInSector_[s] = lolog::kHeaderSize;
        rrIdx = (s + 1) % n;
        if (!writeSectorHeader(s, runSeq, lolog::kSegIndex))
            return false;
        sectorOut = s;
        return true;
    }
    return false;
}

bool LoLog::flushIndexRun()
{
    uint32_t sector = 0;
    const uint32_t runSeq = nextSeq_++;
    if (!allocIndexSector(sector, runSeq))
        return false;

    std::vector<uint8_t> body;
    body.resize(32);
    body[0] = (uint8_t)kIndexBodyTag;
    body[1] = (uint8_t)(kIndexBodyTag >> 8);
    body[2] = (uint8_t)(kIndexBodyTag >> 16);
    body[3] = (uint8_t)(kIndexBodyTag >> 24);
    const uint32_t cov = nextSeq_ > 1 ? nextSeq_ - 1 : 0;
    memcpy(body.data() + 4, &cov, 4);
    memcpy(body.data() + 8, &nextDirId_, 4);
    memcpy(body.data() + 12, &nextInodeId_, 4);
    memcpy(body.data() + 16, &liveBytes_, 8);
    const uint32_t nNames = (uint32_t)nameIndex_.size();
    const uint32_t nDead = (uint32_t)deadDirIds_.size();
    memcpy(body.data() + 24, &nNames, 4);
    memcpy(body.data() + 28, &nDead, 4);

    for (const auto &p : nameIndex_) {
        const uint32_t dirId = p.first.dirId;
        const uint32_t nameLen = (uint32_t)p.first.name.size();
        const uint8_t isDir = p.second.isDir ? 1 : 0;
        const uint32_t id = p.second.id;
        size_t o = body.size();
        body.resize(o + 4 + 4 + nameLen + 1 + 1 + 4);
        memcpy(body.data() + o, &dirId, 4);
        o += 4;
        memcpy(body.data() + o, &nameLen, 4);
        o += 4;
        memcpy(body.data() + o, p.first.name.c_str(), nameLen + 1);
        o += nameLen + 1;
        body.data()[o++] = isDir;
        memcpy(body.data() + o, &id, 4);
    }
    for (uint32_t d : deadDirIds_) {
        size_t o = body.size();
        body.resize(o + 4);
        memcpy(body.data() + o, &d, 4);
    }
    uint32_t nDirParent = 0;
    for (size_t i = 1; i < dirParent_.size(); i++)
        nDirParent++;
    size_t oDp = body.size();
    body.resize(oDp + 4);
    memcpy(body.data() + oDp, &nDirParent, 4);
    for (size_t i = 1; i < dirParent_.size(); i++) {
        uint32_t dirId = (uint32_t)i;
        uint32_t parent = dirParent_[i];
        size_t o = body.size();
        body.resize(o + 8);
        memcpy(body.data() + o, &dirId, 4);
        memcpy(body.data() + o + 4, &parent, 4);
    }

    const uint32_t bodyCrc = crc32(body.data(), body.size());
    body.resize(body.size() + 4);
    memcpy(body.data() + body.size() - 4, &bodyCrc, 4);

    uint32_t off = lolog::kHeaderSize;
    const uint32_t padded = (uint32_t)((body.size() + 3) & ~3u);
    if (off + padded > lolog::kSectorSize)
        return false;
    std::vector<uint8_t> pad(padded, 0xFF);
    memcpy(pad.data(), body.data(), body.size());
    if (!progBytes(sector * lolog::kSectorSize + off, pad.data(), padded))
        return false;
    liveInSector_[sector] = off + padded;
    liveBytes_ += padded;
    coveredSeq_ = cov;
    indexRunSeqs_.push_back(runSeq);
    memtableBytes_ = 0;
    lastFlushMs_ = nowMs();
    mergeIndexRunsIfNeeded();
    return true;
}

void LoLog::mergeIndexRunsIfNeeded()
{
    if (indexRunSeqs_.size() <= lolog::kMaxRuns)
        return;
    const uint32_t keep = indexRunSeqs_.back();
    indexRunSeqs_.clear();
    indexRunSeqs_.push_back(keep);
}

bool LoLog::loadNewestIndexRun()
{
    uint32_t bestSeq = 0;
    uint32_t bestSec = 0xFFFFFFFF;
    for (uint32_t s = 0; s < dev_.sectorCount(); s++) {
        uint32_t magic, ep, seq;
        uint8_t kind = 0;
        if (!readSectorHeader(s, magic, ep, seq, kind))
            continue;
        if (ep != epoch_ || kind != lolog::kSegIndex)
            continue;
        if (seq >= bestSeq) {
            bestSeq = seq;
            bestSec = s;
        }
    }
    if (bestSec == 0xFFFFFFFF)
        return false;

    std::vector<uint8_t> body(lolog::kSectorSize - lolog::kHeaderSize);
    if (!readBytes(bestSec * lolog::kSectorSize + lolog::kHeaderSize, body.data(), body.size()))
        return false;
    uint32_t tag = (uint32_t)body[0] | ((uint32_t)body[1] << 8) | ((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 24);
    if (tag != kIndexBodyTag)
        return false;
    uint32_t cov = 0, nd = 0, ni = 0, nNames = 0, nDead = 0;
    memcpy(&cov, body.data() + 4, 4);
    memcpy(&nd, body.data() + 8, 4);
    memcpy(&ni, body.data() + 12, 4);
    memcpy(&liveBytes_, body.data() + 16, 8);
    memcpy(&nNames, body.data() + 24, 4);
    memcpy(&nDead, body.data() + 28, 4);
    coveredSeq_ = cov;
    nextDirId_ = nd;
    nextInodeId_ = ni;
    size_t o = 32;
    nameIndex_.clear();
    deadDirIds_.clear();
    for (uint32_t i = 0; i < nNames && o + 13 < body.size(); i++) {
        uint32_t dirId = 0, nameLen = 0, id = 0;
        memcpy(&dirId, body.data() + o, 4);
        o += 4;
        memcpy(&nameLen, body.data() + o, 4);
        o += 4;
        if (o + nameLen + 1 + 1 + 4 > body.size())
            break;
        std::string name((char *)(body.data() + o), nameLen);
        o += nameLen + 1;
        bool isDir = body[o++] != 0;
        memcpy(&id, body.data() + o, 4);
        o += 4;
        upsertName(dirId, name.c_str(), NameVal{isDir, id});
    }
    for (uint32_t i = 0; i < nDead && o + 4 <= body.size(); i++) {
        uint32_t d = 0;
        memcpy(&d, body.data() + o, 4);
        o += 4;
        deadDirIds_.push_back(d);
    }
    uint32_t nDirParent = 0;
    if (o + 4 <= body.size()) {
        memcpy(&nDirParent, body.data() + o, 4);
        o += 4;
    }
    for (uint32_t i = 0; i < nDirParent && o + 8 <= body.size(); i++) {
        uint32_t dirId = 0, parent = 0;
        memcpy(&dirId, body.data() + o, 4);
        memcpy(&parent, body.data() + o + 4, 4);
        o += 8;
        if (dirId >= dirParent_.size())
            dirParent_.resize(dirId + 1, lolog::kRootDirId);
        dirParent_[dirId] = parent;
    }
    return true;
}

bool LoLog::copyLiveGroupsFromSector(uint32_t sector)
{
    uint32_t magic, epoch, seq;
    uint8_t kind = 0;
    if (!readSectorHeader(sector, magic, epoch, seq, kind) || kind != lolog::kSegData)
        return false;
    if (epoch != epoch_)
        return false;

    uint32_t off = lolog::kHeaderSize;
    std::vector<PendingEnt> group;
    while (off + 8 <= lolog::kSectorSize) {
        uint8_t hdr[8];
        if (!readBytes(sector * lolog::kSectorSize + off, hdr, 8))
            break;
        uint16_t plen = (uint16_t)hdr[0] | ((uint16_t)hdr[1] << 8);
        if (plen == lolog::kEntryEnd)
            break;
        uint8_t type = hdr[2];
        uint8_t flags = hdr[3];
        uint32_t padded = (8 + plen + 3) & ~3u;
        if (off + padded > lolog::kSectorSize)
            break;
        std::vector<uint8_t> payload(plen);
        if (plen && !readBytes(sector * lolog::kSectorSize + off + 4, payload.data(), plen))
            break;
        group.push_back({type, std::move(payload)});
        off += padded;
        if (flags & lolog::kFlagGroupEnd) {
            beginGroup();
            for (auto &e : group)
                pendingGroup_.push_back(std::move(e));
            group.clear();
            if (!endGroup())
                return false;
        }
    }
    return true;
}

void LoLog::cleanOneSector()
{
    if (freeSectors() <= lolog::kHeadroomSectors)
        return;
    uint32_t best = 0xFFFFFFFF;
    uint32_t bestDead = 0;
    uint32_t bestEc = UINT32_MAX;
    for (uint32_t s = 0; s < sectorUsed_.size(); s++) {
        if (!sectorUsed_[s] || s == activeSector_)
            continue;
        uint32_t magic, ep, seq;
        uint8_t kind = 0;
        if (!readSectorHeader(s, magic, ep, seq, kind) || kind != lolog::kSegData)
            continue;
        uint32_t live = liveInSector_[s];
        uint32_t dead = lolog::kSectorSize > live ? lolog::kSectorSize - live : 0;
        uint32_t ec = dev_.eraseCount(s);
        if (dead > bestDead || (dead == bestDead && ec < bestEc)) {
            bestDead = dead;
            bestEc = ec;
            best = s;
        }
    }
    if (best == 0xFFFFFFFF || bestDead < lolog::kSectorSize / 4)
        return;
    if (!copyLiveGroupsFromSector(best))
        return;
    const uint32_t freed = liveInSector_[best];
    dev_.eraseSector(best);
    sectorUsed_[best] = 0;
    liveInSector_[best] = 0;
    if (liveBytes_ >= freed)
        liveBytes_ -= freed;
}

void LoLog::flushIndexIfNeeded()
{
    memtableBytes_ = (uint32_t)(nameIndex_.size() * 48);
    const uint32_t now = nowMs();
    if (memtableBytes_ >= lolog::kMemtableBytes ||
        (lastFlushMs_ && now - lastFlushMs_ > kIdleFlushMs))
        flushIndexRun();
}

void LoLog::maintain(uint32_t budgetMs)
{
    (void)budgetMs;
    flushIndexIfNeeded();
    cleanOneSector();
}

#include "core/LoBBSStackGuard.h"
