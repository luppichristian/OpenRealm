// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IPlayerRegistry
{
  function isRegistered(address account) external view returns (bool);

  // Stable numeric id used by off-chain/runtime code as a compact player reference.
  function playerIdOf(address account) external view returns (uint256);

  // Resolves either a wallet or an authorized runtime session key back to the wallet account.
  function resolveRuntimeAccount(address actor) external view returns (address account, bool isDirect, bool isRuntimeSession);
}
