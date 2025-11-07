#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

/**
 * CommandParser - Parse text commands from mesh packets
 *
 * Commands start with '/' followed by command name, then optional arguments.
 * Example: "/hi alice mypassword"
 */
class CommandParser
{
  public:
    /**
     * Initialize parser with a payload buffer
     * @param buffer Pointer to the payload bytes
     * @param size Size of the payload
     */
    CommandParser(const uint8_t *buffer, size_t size);

    /**
     * Check if this is a command (starts with '/')
     * @return true if payload starts with '/'
     */
    bool isCommand() const;

    /**
     * Get the command name (everything after '/' until first space or end)
     * @param buffer Buffer to store command name (null-terminated)
     * @param bufferSize Size of the buffer
     * @return true if command name extracted successfully
     */
    bool commandName(char *buffer, size_t bufferSize) const;

    /**
     * Get the next word from the current position
     * Advances internal position pointer
     * @param buffer Buffer to store the word (null-terminated)
     * @param bufferSize Size of the buffer
     * @return true if word extracted successfully, false if no more words
     */
    bool nextWord(char *buffer, size_t bufferSize);

    /**
     * Get the rest of the string from current position
     * @param buffer Buffer to store the rest (null-terminated)
     * @param bufferSize Size of the buffer
     * @return true if there's content remaining
     */
    bool rest(char *buffer, size_t bufferSize) const;

    /**
     * Reset the parser position to start of arguments (after command name)
     */
    void reset();

    /**
     * Check if there are more arguments to parse
     * @return true if more content available
     */
    bool hasMore() const;

  private:
    const uint8_t *payload;
    size_t payloadSize;
    size_t position;     // Current parsing position
    size_t argsStartPos; // Position where arguments start (after command name)
};
