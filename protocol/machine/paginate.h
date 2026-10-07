#pragma once
#include <stdint.h>
#include <string>

bool lobbsPaginateMachine(uint32_t reqId, const std::string &document, uint32_t page1, std::string &pageOut, const char **errMsg);

