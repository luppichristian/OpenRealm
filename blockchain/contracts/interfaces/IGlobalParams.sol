// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IGlobalParams
{
  function MIN_CHUNK_COORD() external view returns (int32);

  function MAX_CHUNK_COORD() external view returns (int32);

  function MIN_CHUNK_PRICE() external view returns (uint256);

  function MAX_FEE_BPS() external view returns (uint96);

  function MIN_AUCTION_DURATION() external view returns (uint64);
}
