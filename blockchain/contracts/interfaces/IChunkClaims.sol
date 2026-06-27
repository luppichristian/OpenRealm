// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IChunkClaims
{
  function ownerOf(int32 x, int32 y) external view returns (address);

  function tokenIdOfChunk(int32 x, int32 y) external view returns (uint256);

  // Used by marketplace flows to reject buyers that are not registered players.
  function isRegisteredPlayer(address account) external view returns (bool);

  // Special transfer entrypoint reserved for the marketplace contract.
  function marketplaceTransferChunk(
    int32 x,
    int32 y,
    address from,
    address to
  ) external;
}
