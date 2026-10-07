#include <cstdio>
#include <loutil/LoUtil.h>

const char *loU64ToDec(uint64_t value, char (&buf)[LO_U64_DEC_LEN])
{
    char *p = buf + LO_U64_DEC_LEN - 1;
    *p = '\0';
    do {
        *--p = (char)('0' + (value % 10));
        value /= 10;
    } while (value);
    return p;
}

const char *loHumanBytes(uint64_t bytes, char (&buf)[LO_HUMAN_BYTES_LEN])
{
    static const char units[] = "KMGT";
    if (bytes < 1024) {
        snprintf(buf, LO_HUMAN_BYTES_LEN, "%uB", (unsigned)bytes);
        return buf;
    }
    uint64_t div = 1024;
    int u = 0;
    while (u < 3 && bytes >= div * 1024) {
        div *= 1024;
        u++;
    }
    const unsigned tenths = (unsigned)((bytes * 10 + div / 2) / div);
    if (tenths < 100 && tenths % 10)
        snprintf(buf, LO_HUMAN_BYTES_LEN, "%u.%u%c", tenths / 10, tenths % 10, units[u]);
    else
        snprintf(buf, LO_HUMAN_BYTES_LEN, "%u%c", (tenths + 5) / 10, units[u]);
    return buf;
}
