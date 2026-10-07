#pragma once
// Include after the other includes in every LoBBS .cpp. Commands run on the nRF52 loop task, whose 4 KB stack
// is shared with the router and has no overflow trap, so each LoBBS function is capped at compile time.
#if defined(ARDUINO_ARCH_NRF52) && defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic error "-Wstack-usage=512"
#endif
