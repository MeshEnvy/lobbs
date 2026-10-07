#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <stdint.h>
#include <string>

bool lobbsPaginatePlainText(const std::string &text, uint32_t page1, std::string &pageOut, const char **errMsg);

#endif
