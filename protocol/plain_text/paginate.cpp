#include "paginate.h"
#include "LoBBSReply.h"
#include <cstdio>
#include <cstring>
#include <vector>

#include "LoBBSStackGuard.h"

static constexpr size_t LOBBS_PAGER_FOOTER_MAX = 16;
static constexpr int LOBBS_PAGER_MAX_PAGES = 64;
static constexpr char LOBBS_LINE_TRUNC[] = "[...]";
static constexpr size_t LOBBS_LINE_TRUNC_LEN = sizeof(LOBBS_LINE_TRUNC) - 1;

static size_t lobbsHumanLineLen(const std::string &line, size_t maxBytes)
{
    size_t len = strlen(line.c_str());
    if (len <= maxBytes)
        return len;
    return maxBytes <= LOBBS_LINE_TRUNC_LEN ? 0 : maxBytes;
}

static void lobbsHumanAppendLine(std::string &out, const std::string &line, size_t maxBytes)
{
    size_t len = strlen(line.c_str());
    if (len <= maxBytes) {
        out.append(line, 0, len);
        return;
    }
    if (maxBytes <= LOBBS_LINE_TRUNC_LEN)
        return;
    out.append(line, 0, maxBytes - LOBBS_LINE_TRUNC_LEN);
    out += LOBBS_LINE_TRUNC;
}

static bool lobbsHumanPackPages(const std::vector<std::string> &lines, size_t maxBytes, bool reserveFooter, uint32_t *pageStarts,
                                int &pageCountOut)
{
    pageCountOut = 0;
    if (lines.empty())
        return false;
    uint32_t idx = 0;
    while (idx < lines.size() && pageCountOut < LOBBS_PAGER_MAX_PAGES) {
        pageStarts[pageCountOut] = idx;
        size_t used = 0;
        while (idx < lines.size()) {
            size_t footerReserve = reserveFooter ? LOBBS_PAGER_FOOTER_MAX : 0;
            size_t maxLine = maxBytes > footerReserve ? maxBytes - footerReserve : 0;
            size_t lineLen = lobbsHumanLineLen(lines[idx], maxLine);
            size_t add = lineLen + (used > 0 ? 1 : 0);
            if (used + add + footerReserve > maxBytes) {
                if (used > 0)
                    break;
                used += lineLen;
                idx++;
                break;
            }
            used += add;
            idx++;
            if (idx < lines.size()) {
                size_t nextAdd = lobbsHumanLineLen(lines[idx], maxLine) + 1;
                if (used + nextAdd + footerReserve > maxBytes)
                    break;
            }
        }
        pageCountOut++;
    }
    if (pageCountOut < LOBBS_PAGER_MAX_PAGES)
        pageStarts[pageCountOut] = (uint32_t)lines.size();
    return pageCountOut > 0;
}

bool lobbsPaginatePlainText(const std::string &text, uint32_t page1, std::string &pageOut, const char **errMsg)
{
    pageOut.clear();
    if (text.empty()) {
        if (errMsg)
            *errMsg = "Empty.";
        return false;
    }

    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, end - start));
        start = end + 1;
    }

    if (page1 == 0)
        page1 = 1;
    uint32_t pageStarts[LOBBS_PAGER_MAX_PAGES + 1];
    memset(pageStarts, 0, sizeof(pageStarts));
    int pageCount = 0;
    lobbsHumanPackPages(lines, LOBBS_REPLY_BYTES, true, pageStarts, pageCount);
    if (pageCount <= 0) {
        if (errMsg)
            *errMsg = "Empty.";
        return false;
    }
    if (pageCount == 1) {
        lobbsHumanPackPages(lines, LOBBS_REPLY_BYTES, false, pageStarts, pageCount);
        pageStarts[1] = (uint32_t)lines.size();
    }
    if (page1 > (uint32_t)pageCount) {
        if (errMsg)
            *errMsg = "No such page.";
        return false;
    }

    uint32_t from = pageStarts[page1 - 1];
    uint32_t to = pageStarts[page1];
    size_t footerReserve = pageCount > 1 ? LOBBS_PAGER_FOOTER_MAX : 0;
    size_t maxLine = LOBBS_REPLY_BYTES > footerReserve ? LOBBS_REPLY_BYTES - footerReserve : 0;
    for (uint32_t i = from; i < to; i++) {
        if (i > from)
            pageOut.push_back('\n');
        lobbsHumanAppendLine(pageOut, lines[i], maxLine);
    }
    if ((uint32_t)pageCount > 1) {
        char footer[LOBBS_PAGER_FOOTER_MAX];
        snprintf(footer, sizeof(footer), "{p %u/%u}", page1, (uint32_t)pageCount);
        pageOut.push_back('\n');
        pageOut += footer;
    }
    return true;
}

