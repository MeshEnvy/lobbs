#pragma once
#include "LoBBSArch.h"

#define LOBBS_MAX_USERNAME_LEN 32
#define LOBBS_USERNAME_BUFFER_SIZE (LOBBS_MAX_USERNAME_LEN + 1)

/** Mail/news body stored in LoDB (LOBBS_REPLY_BYTES caps the DM). */
#define LOBBS_MESSAGE_BODY_MAX 200
#define LOBBS_MESSAGE_BODY_BUFFER_SIZE (LOBBS_MESSAGE_BODY_MAX + 1)
/** Body excerpt in a read reply (header + body must fit LOBBS_REPLY_BYTES). */
#define LOBBS_MESSAGE_READ_BODY_MAX 120
#define LOBBS_MESSAGE_READ_BODY_BUFFER_SIZE (LOBBS_MESSAGE_READ_BODY_MAX + 1)
/** `lobbsAppTimeAgo` (e.g. "999999d ago"). */
#define LOBBS_TIME_AGO_BUFFER_SIZE 32
/** Inbox/news list line: truncated preview of the message. */
#define LOBBS_LIST_LINE_TRUNC_BUFFER_SIZE 50
#define LOBBS_LIST_LINE_TRUNC_MAX_CHARS 25
/** SysOp working directory for /cd (held in the RAM session slot). */
#define LOBBS_CWD_BUFFER_SIZE 128
/** Shared in /hi and /help root (omit topic for list, or name a command). */
#define LOBBS_HELP_HINT "Use /help [topic] for help"

#ifndef LOBBS_EXTRA_QSPI
#define LOBBS_EXTRA_QSPI 0
#endif

#ifndef LOFS_ENABLE_EXTRA_QSPI
#define LOFS_ENABLE_EXTRA_QSPI LOBBS_EXTRA_QSPI
#endif

#if LOBBS_ARCH_NRF52
#define LOFS_NRF52 1
#ifndef LOBBS_NRF52_FLASH_PAGE_SIZE
#ifdef FLASH_NRF52_PAGE_SIZE
#define LOBBS_NRF52_FLASH_PAGE_SIZE FLASH_NRF52_PAGE_SIZE
#else
#define LOBBS_NRF52_FLASH_PAGE_SIZE 4096
#endif
#endif
#endif

#if defined(LOBBS_DEMO_MODE) || defined(PIO_UNIT_TESTING)
#define LOBBS_SEED 1
#endif

