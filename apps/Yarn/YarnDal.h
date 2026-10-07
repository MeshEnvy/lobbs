#pragma once
#if !MESHTASTIC_EXCLUDE_LOBBS

#include <lodb/LoDB.h>
#include <stddef.h>
#include <stdint.h>

static constexpr size_t LOBBS_YARN_BODY_MAX = 186;

static constexpr uint32_t LOBBS_YARN_DEFAULT_PERIOD_SEC = 3600;
static constexpr uint32_t LOBBS_YARN_DEFAULT_MAX_WORDS = 1;
static constexpr uint32_t LOBBS_YARN_DEFAULT_MAX_CHARS = 32;

class YarnDal
{
  public:
    explicit YarnDal(LoDb &lodb);

    bool formatYarnView(char *out, size_t outCap);
    uint32_t newWordsForUser(uint64_t userUuid);
    bool markYarnSeen(uint64_t userUuid);
    /** Returns nullptr on success, else a user-facing error. */
    const char *appendWords(uint64_t userUuid, bool isSysop, const char *const *words, int wordCount, uint32_t periodSeconds,
                            uint32_t maxWords, uint32_t maxChars);
    uint32_t totalWordsAppended();

  private:
    struct CurrentState {
        char text[LOBBS_YARN_BODY_MAX + 1];
        uint32_t total_words_appended;
    };
    void loadCurrent(CurrentState &out);
    LoDbError saveCurrent(CurrentState &cur);
    static void trimTailToMax(char *text);
    static bool isValidWordToken(const char *word);
    LoDb &lodb_;
};

#endif
