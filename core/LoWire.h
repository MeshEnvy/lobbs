#pragma once
#include <stddef.h>
#include <stdint.h>

/** Session and reply routing key. Meshtastic uses node num; MeshCore uses a stable hash of the contact key. */
struct LoNodeId {
    uint32_t key = 0;
};

/** Local = trusted operator path (USB console on MeshCore, phone/API client on Meshtastic). */
enum class LoInboundChannel : uint8_t { Radio = 0, Local = 1 };

/** One inbound text line or DM payload. */
struct LoInbound {
    LoNodeId from;
    LoInboundChannel channel = LoInboundChannel::Radio;
    const char *text = nullptr;
    /** Platform-owned reply context (Meshtastic MeshPacket, MeshCore ContactInfo, etc.). */
    const void *replyContext = nullptr;
};
