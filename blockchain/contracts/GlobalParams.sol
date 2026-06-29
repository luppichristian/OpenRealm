// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

error InvalidChunkCoordRange(int32 minChunkCoord, int32 maxChunkCoord);
error InvalidChunkPrice(uint256 minChunkPrice);
error InvalidFeeBpsLimit(uint96 maxFeeBps);
error InvalidAuctionDurationLimit(uint64 minAuctionDuration);

contract GlobalParams
{
  int32 public immutable MIN_CHUNK_COORD;
  int32 public immutable MAX_CHUNK_COORD;
  uint256 public immutable MIN_CHUNK_PRICE;
  uint96 public immutable MAX_FEE_BPS;
  uint64 public immutable MIN_AUCTION_DURATION;

  constructor(
    int32 minChunkCoord,
    int32 maxChunkCoord,
    uint256 minChunkPrice,
    uint96 maxFeeBps,
    uint64 minAuctionDuration
  )
  {
    if (minChunkCoord > maxChunkCoord)
    {
      revert InvalidChunkCoordRange(minChunkCoord, maxChunkCoord);
    }
    if (minChunkPrice == 0)
    {
      revert InvalidChunkPrice(minChunkPrice);
    }
    if (maxFeeBps > 10_000)
    {
      revert InvalidFeeBpsLimit(maxFeeBps);
    }
    if (minAuctionDuration == 0)
    {
      revert InvalidAuctionDurationLimit(minAuctionDuration);
    }

    MIN_CHUNK_COORD = minChunkCoord;
    MAX_CHUNK_COORD = maxChunkCoord;
    MIN_CHUNK_PRICE = minChunkPrice;
    MAX_FEE_BPS = maxFeeBps;
    MIN_AUCTION_DURATION = minAuctionDuration;
  }
}
