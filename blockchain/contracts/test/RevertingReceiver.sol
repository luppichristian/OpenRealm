// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

interface IRevertingReceiverPlayerRegistry
{
  function Register(string calldata handle, string calldata metadataURI) external;
}

interface IRevertingReceiverChunkClaims
{
  function ClaimChunk(int32 x, int32 y) external payable;
}

interface IRevertingReceiverMarketplace
{
  function CreateListing(int32 x, int32 y, uint256 price) external returns (uint256 listingId);
  function CreateAuction(
    int32 x,
    int32 y,
    uint256 reservePrice,
    uint256 minBidIncrement,
    uint64 durationSeconds
  )
    external
    returns (uint256 auctionId);
  function PlaceAuctionBid(uint256 auctionId) external payable;
  function WithdrawBalance(address payable recipient) external;
}

contract RevertingReceiver
{
  IRevertingReceiverPlayerRegistry public immutable registry;
  IRevertingReceiverChunkClaims public immutable claims;
  IRevertingReceiverMarketplace public immutable marketplace;

  constructor(address registryAddress, address claimsAddress, address marketplaceAddress)
  {
    registry = IRevertingReceiverPlayerRegistry(registryAddress);
    claims = IRevertingReceiverChunkClaims(claimsAddress);
    marketplace = IRevertingReceiverMarketplace(marketplaceAddress);
  }

  receive() external payable
  {
    revert("REJECT_ETHER");
  }

  function Register(string calldata handle, string calldata metadataURI) external
  {
    registry.Register(handle, metadataURI);
  }

  function ClaimChunk(int32 x, int32 y) external payable
  {
    claims.ClaimChunk{value: msg.value}(x, y);
  }

  function CreateListing(int32 x, int32 y, uint256 price) external
  {
    marketplace.CreateListing(x, y, price);
  }

  function CreateAuction(
    int32 x,
    int32 y,
    uint256 reservePrice,
    uint256 minBidIncrement,
    uint64 durationSeconds
  )
    external
  {
    marketplace.CreateAuction(x, y, reservePrice, minBidIncrement, durationSeconds);
  }

  function PlaceAuctionBid(uint256 auctionId) external payable
  {
    marketplace.PlaceAuctionBid{value: msg.value}(auctionId);
  }

  function WithdrawMarketplaceBalance(address payable recipient) external
  {
    marketplace.WithdrawBalance(recipient);
  }
}
