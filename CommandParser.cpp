#include "CommandParser.h"
#include <cctype>
#include <cstdint>

CommandParser::CommandParser(const uint8_t *buffer, size_t size)
    : payload(buffer), payloadSize(size), position(0), argsStartPos(0)
{
    // Skip the '/' if present and find where arguments start
    if (isCommand() && payloadSize > 1) {
        // Find the first space after the command name
        size_t pos = 1; // Start after '/'
        while (pos < payloadSize && payload[pos] != ' ' && payload[pos] != '\0') {
            pos++;
        }
        // Skip the space
        while (pos < payloadSize && payload[pos] == ' ') {
            pos++;
        }
        argsStartPos = pos;
        position = pos;
    }
}

bool CommandParser::isCommand() const
{
    return payloadSize > 0 && payload[0] == '/';
}

bool CommandParser::commandName(char *buffer, size_t bufferSize) const
{
    if (!isCommand() || bufferSize == 0) {
        return false;
    }

    // Start after '/'
    size_t srcPos = 1;
    size_t dstPos = 0;

    // Copy until space, null terminator, or end of payload
    while (srcPos < payloadSize && dstPos < bufferSize - 1) {
        char c = payload[srcPos];
        if (c == ' ' || c == '\0') {
            break;
        }
        buffer[dstPos++] = c;
        srcPos++;
    }

    buffer[dstPos] = '\0';
    return dstPos > 0;
}

bool CommandParser::nextWord(char *buffer, size_t bufferSize)
{
    if (bufferSize == 0 || position >= payloadSize) {
        return false;
    }

    // Skip leading spaces
    while (position < payloadSize && payload[position] == ' ') {
        position++;
    }

    if (position >= payloadSize) {
        return false;
    }

    // Copy the word
    size_t dstPos = 0;
    while (position < payloadSize && dstPos < bufferSize - 1) {
        char c = payload[position];
        if (c == ' ' || c == '\0') {
            break;
        }
        buffer[dstPos++] = c;
        position++;
    }

    buffer[dstPos] = '\0';
    return dstPos > 0;
}

bool CommandParser::rest(char *buffer, size_t bufferSize) const
{
    if (bufferSize == 0 || position >= payloadSize) {
        buffer[0] = '\0';
        return false;
    }

    // Skip leading spaces
    size_t srcPos = position;
    while (srcPos < payloadSize && payload[srcPos] == ' ') {
        srcPos++;
    }

    if (srcPos >= payloadSize) {
        buffer[0] = '\0';
        return false;
    }

    // Copy the rest
    size_t dstPos = 0;
    while (srcPos < payloadSize && dstPos < bufferSize - 1) {
        char c = payload[srcPos];
        if (c == '\0') {
            break;
        }
        buffer[dstPos++] = c;
        srcPos++;
    }

    buffer[dstPos] = '\0';
    return dstPos > 0;
}

void CommandParser::reset()
{
    position = argsStartPos;
}

bool CommandParser::hasMore() const
{
    // Check if there's non-space content remaining
    size_t pos = position;
    while (pos < payloadSize && payload[pos] == ' ') {
        pos++;
    }
    return pos < payloadSize && payload[pos] != '\0';
}
