#pragma once

#include <cstdint>

// Bump when the runtime handshake / packet semantics become incompatible.
static constexpr uint32_t kRuntimeProtocolVersion = 1;

// Bump when the packet header/wire framing itself changes.
static constexpr uint8_t kRuntimePacketVersion = 1;
