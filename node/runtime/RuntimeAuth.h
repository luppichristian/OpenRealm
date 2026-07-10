#pragma once

#include "../blockchain/Wallet.h"

#include <cstdint>
#include <string>
#include <vector>

bool ResolveRuntimeWalletAddress(const Wallet& wallet, std::string* signerAddress);
bool SignRuntimeAuthMessage(const Wallet& wallet, const std::vector<uint8_t>& messageBytes, std::string* signerAddress, std::string* signatureHex);
bool RecoverRuntimeAuthSigner(const std::vector<uint8_t>& messageBytes, const std::string& signatureHex, std::string* signerAddress);
std::string NormalizeRuntimeAuthAddress(const std::string& address);
