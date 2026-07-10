#pragma once

#include <cstdint>

// Bump when the runtime topology / join semantics become incompatible.
static constexpr uint32_t kRuntimeProtocolVersion = 4;

// Bump when the packet header / wire framing changes.
static constexpr uint8_t kRuntimePacketVersion = 4;
