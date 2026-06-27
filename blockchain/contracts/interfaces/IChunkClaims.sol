// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IChunkClaims
{
  function OwnerOf(int32 x, int32 y) external view returns (address);

  function TokenIdOfChunk(int32 x, int32 y) external view returns (uint256);

  // Used by marketplace flows to reject buyers that are not registered players.
  function IsRegisteredPlayer(address account) external view returns (bool);

  // Special transfer entrypoint reserved for the marketplace contract.
  function MarketplaceTransferChunk(
    int32 x,
    int32 y,
    address from,
    address to
  ) external;
}
