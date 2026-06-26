// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IOpenRealmChunkClaims
{
  function ownerOf(int32 x, int32 y) external view returns (address);

  function tokenIdOfChunk(int32 x, int32 y) external view returns (uint256);

  function isRegisteredPlayer(address account) external view returns (bool);

  function marketplaceTransferChunk(
    int32 x,
    int32 y,
    address from,
    address to
  ) external;
}
