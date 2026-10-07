#include <algorithm>
#include <cstdio>
#include <cstring>
#include <loscalar/LoScalar.h>
#include <loutil/LoUtil.h>

#include "LoBBSStackGuard.h"

static constexpr uint32_t kLoScalarFieldMax = 99;

static bool loscalarFieldOk(uint32_t field)
{
    return field <= kLoScalarFieldMax;
}

namespace
{

bool loscalarUnescapeValue(const char *in, size_t inLen, std::string &raw)
{
    raw.clear();
    raw.reserve(inLen);
    for (size_t i = 0; i < inLen; i++) {
        if (in[i] == '\\' && i + 1 < inLen) {
            char n = in[i + 1];
            if (n == '\\') {
                raw.push_back('\\');
                i++;
            } else if (n == '|') {
                raw.push_back('|');
                i++;
            } else if (n == 'n') {
                raw.push_back('\n');
                i++;
            } else
                return false;
        } else
            raw.push_back(in[i]);
    }
    return true;
}

} // namespace

void LoScalar::clear()
{
    fields_.clear();
}

static bool fieldLess(const LoScalar::Field &a, const LoScalar::Field &b)
{
    return a.number < b.number;
}

LoScalar::Field *LoScalar::findOrInsert(uint32_t field)
{
    if (!loscalarFieldOk(field))
        return nullptr;
    for (auto &f : fields_) {
        if (f.number == field)
            return &f;
    }
    Field nf;
    nf.number = field;
    fields_.push_back(nf);
    return &fields_.back();
}

const LoScalar::Field *LoScalar::find(uint32_t field) const
{
    for (const auto &f : fields_) {
        if (f.number == field)
            return &f;
    }
    return nullptr;
}

void LoScalar::setString(uint32_t field, const char *value)
{
    if (!value || !value[0])
        return;
    Field *f = findOrInsert(field);
    if (f)
        f->value = value;
}

void LoScalar::setString(uint32_t field, const std::string &value)
{
    if (value.empty())
        return;
    Field *f = findOrInsert(field);
    if (f)
        f->value = value;
}

void LoScalar::setUint64(uint32_t field, uint64_t value)
{
    char buf[LO_U64_DEC_LEN];
    const char *digits = loU64ToDec(value, buf);
    Field *f = findOrInsert(field);
    if (f)
        f->value = digits;
}

void LoScalar::setUint32(uint32_t field, uint32_t value)
{
    setUint64(field, value);
}

void LoScalar::setBool(uint32_t field, bool value)
{
    Field *f = findOrInsert(field);
    if (f)
        f->value = value ? "1" : "0";
}

void LoScalar::removeField(uint32_t field)
{
    if (!loscalarFieldOk(field))
        return;
    for (auto it = fields_.begin(); it != fields_.end(); ++it) {
        if (it->number == field) {
            fields_.erase(it);
            return;
        }
    }
}

void LoScalar::setBytesHex(uint32_t field, const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return;
    std::string hex;
    hex.reserve(len * 2);
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        hex.push_back(digits[(data[i] >> 4) & 0xf]);
        hex.push_back(digits[data[i] & 0xf]);
    }
    Field *f = findOrInsert(field);
    if (f)
        f->value = hex;
}

bool LoScalar::has(uint32_t field) const
{
    return find(field) != nullptr;
}

bool LoScalar::getString(uint32_t field, std::string &out) const
{
    const Field *f = find(field);
    if (!f)
        return false;
    out = f->value;
    return true;
}

bool LoScalar::getUint64(uint32_t field, uint64_t &out) const
{
    const Field *f = find(field);
    if (!f || f->value.empty())
        return false;
    char *end = nullptr;
    unsigned long long v = strtoull(f->value.c_str(), &end, 10);
    if (end == f->value.c_str() || (end && *end != '\0'))
        return false;
    out = v;
    return true;
}

bool LoScalar::getUint32(uint32_t field, uint32_t &out) const
{
    uint64_t v = 0;
    if (!getUint64(field, v))
        return false;
    out = (uint32_t)v;
    return true;
}

bool LoScalar::getBool(uint32_t field, bool &out) const
{
    const Field *f = find(field);
    if (!f)
        return false;
    if (f->value == "1") {
        out = true;
        return true;
    }
    if (f->value == "0") {
        out = false;
        return true;
    }
    return false;
}

bool LoScalar::getBytesHex(uint32_t field, uint8_t *out, size_t outLen, size_t &written) const
{
    written = 0;
    const Field *f = find(field);
    if (!f || f->value.size() % 2 != 0)
        return false;
    size_t byteCount = f->value.size() / 2;
    if (byteCount > outLen)
        return false;
    for (size_t i = 0; i < byteCount; i++) {
        unsigned hi = 0, lo = 0;
        char c1 = f->value[i * 2];
        char c2 = f->value[i * 2 + 1];
        if (c1 >= '0' && c1 <= '9')
            hi = (unsigned)(c1 - '0');
        else if (c1 >= 'a' && c1 <= 'f')
            hi = (unsigned)(c1 - 'a' + 10);
        else if (c1 >= 'A' && c1 <= 'F')
            hi = (unsigned)(c1 - 'A' + 10);
        else
            return false;
        if (c2 >= '0' && c2 <= '9')
            lo = (unsigned)(c2 - '0');
        else if (c2 >= 'a' && c2 <= 'f')
            lo = (unsigned)(c2 - 'a' + 10);
        else if (c2 >= 'A' && c2 <= 'F')
            lo = (unsigned)(c2 - 'A' + 10);
        else
            return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    written = byteCount;
    return true;
}

void LoScalar::escapeValue(const std::string &raw, std::string &escaped)
{
    escaped.clear();
    escaped.reserve(raw.size());
    for (char c : raw) {
        if (c == '\\')
            escaped.append("\\\\");
        else if (c == '|')
            escaped.append("\\|");
        else if (c == '\n')
            escaped.append("\\n");
        else
            escaped.push_back(c);
    }
}

bool LoScalar::encode(std::string &line, size_t maxBytes) const
{
    line.clear();
    std::vector<Field> sorted = fields_;
    std::sort(sorted.begin(), sorted.end(), fieldLess);
    for (const auto &fld : sorted) {
        if (fld.value.empty())
            continue;
        if (!line.empty())
            line.push_back('|');
        char numBuf[12];
        snprintf(numBuf, sizeof(numBuf), "%u:", (unsigned)fld.number);
        line.append(numBuf);
        std::string esc;
        escapeValue(fld.value, esc);
        line.append(esc);
        if (line.size() > maxBytes)
            return false;
    }
    return true;
}

static bool decodeOneField(const char *line, size_t lineLen, size_t &pos, uint32_t &fieldNum, std::string &value)
{
    fieldNum = 0;
    value.clear();
    if (pos >= lineLen)
        return false;
    if (line[pos] < '0' || line[pos] > '9')
        return false;
    while (pos < lineLen && line[pos] >= '0' && line[pos] <= '9') {
        fieldNum = fieldNum * 10 + (uint32_t)(line[pos] - '0');
        if (fieldNum > kLoScalarFieldMax)
            return false;
        pos++;
    }
    if (pos >= lineLen || line[pos] != ':')
        return false;
    pos++;

    size_t start = pos;
    while (pos < lineLen) {
        if (line[pos] == '\\') {
            pos += 2;
            if (pos > lineLen)
                return false;
            continue;
        }
        if (line[pos] == '|') {
            if (!loscalarUnescapeValue(line + start, pos - start, value))
                return false;
            pos++;
            return true;
        }
        pos++;
    }
    if (!loscalarUnescapeValue(line + start, pos - start, value))
        return false;
    return true;
}

bool LoScalar::decode(const char *line, size_t lineLen)
{
    clear();
    if (!line)
        return false;
    if (lineLen == 0)
        return true;
    size_t pos = 0;
    while (pos < lineLen) {
        uint32_t fieldNum = 0;
        std::string value;
        if (!decodeOneField(line, lineLen, pos, fieldNum, value))
            return false;
        if (!loscalarFieldOk(fieldNum))
            return false;
        Field *f = findOrInsert(fieldNum);
        if (f)
            f->value = value;
        while (pos < lineLen && (line[pos] == ' ' || line[pos] == '\r' || line[pos] == '\n'))
            pos++;
    }
    return true;
}
