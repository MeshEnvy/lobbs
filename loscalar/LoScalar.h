#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/** One LoScalar record: fields `number:value` joined by `|`, ascending field number. */
class LoScalar
{
  public:
    struct Field {
        uint32_t number = 0;
        std::string value;
    };

    void clear();
    void setString(uint32_t field, const char *value);
    void setString(uint32_t field, const std::string &value);
    void setUint64(uint32_t field, uint64_t value);
    void setUint32(uint32_t field, uint32_t value);
    void setBool(uint32_t field, bool value);
    /** 32 bytes encoded as 64 lowercase hex chars. */
    void setBytesHex(uint32_t field, const uint8_t *data, size_t len);
    void removeField(uint32_t field);

    bool has(uint32_t field) const;
    bool getString(uint32_t field, std::string &out) const;
    bool getUint64(uint32_t field, uint64_t &out) const;
    bool getUint32(uint32_t field, uint32_t &out) const;
    bool getBool(uint32_t field, bool &out) const;
    bool getBytesHex(uint32_t field, uint8_t *out, size_t outLen, size_t &written) const;

    /** Encode to a single line (no trailing newline). Returns false if maxBytes would be exceeded. */
    bool encode(std::string &line, size_t maxBytes) const;
    /** Decode one line. Returns false on parse error. */
    bool decode(const char *line, size_t lineLen);

  private:
    Field *findOrInsert(uint32_t field);
    const Field *find(uint32_t field) const;
    static void escapeValue(const std::string &raw, std::string &escaped);

    std::vector<Field> fields_;
};
