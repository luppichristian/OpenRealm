// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import "./interfaces/IChunkClaims.sol";
import "./utils/Ownable.sol";

error InvalidFeeBps(uint96 feeBps);
error InvalidPrice(uint256 price);
error InvalidAuctionDuration(uint64 durationSeconds);
error ListingNotActive(uint256 listingId);
error AuctionNotActive(uint256 auctionId);
error SaleAlreadyExists(uint256 tokenId);
error NotListingSeller(address account, uint256 listingId);
error NotAuctionSeller(address account, uint256 auctionId);
error IncorrectPayment(uint256 expected, uint256 actual);
error SellerNoLongerOwnsChunk(uint256 saleId);
error FeeWithdrawalFailed(address recipient, uint256 amount);
error SellerPayoutFailed(address recipient, uint256 amount);
error BidRefundFailed(address recipient, uint256 amount);
error BidTooLow(uint256 minimumBid, uint256 actualBid);
error AuctionAlreadyEnded(uint256 auctionId);
error AuctionNotEnded(uint256 auctionId);
error AuctionHasExistingBids(uint256 auctionId);
error NotRegisteredBuyer(address account);

contract Marketplace is Ownable
{
  struct Listing
  {
    uint256 id;
    uint256 tokenId;
    int32 x;
    int32 y;
    address seller;
    uint256 price;
    bool active;
  }

  struct Auction
  {
    uint256 id;
    uint256 tokenId;
    int32 x;
    int32 y;
    address seller;
    uint256 reservePrice;
    uint256 minBidIncrement;
    uint64 endTime;
    address highestBidder;
    uint256 highestBid;
    bool active;
    bool settled;
  }

  // Unified read model so runtime/UI code does not need to inspect listings and auctions separately.
  struct SaleState
  {
    uint8 saleKind;
    uint256 saleId;
    uint256 tokenId;
    int32 x;
    int32 y;
    address seller;
    bool sellerStillOwnsChunk;
    bool active;
    uint256 price;
    uint256 reservePrice;
    uint256 minBidIncrement;
    uint64 endTime;
    address highestBidder;
    uint256 highestBid;
  }

  uint8 public constant SALE_KIND_NONE = 0;
  uint8 public constant SALE_KIND_LISTING = 1;
  uint8 public constant SALE_KIND_AUCTION = 2;
  uint96 public constant MAX_FEE_BPS = 2_500;
  uint64 public constant MIN_AUCTION_DURATION = 60;

  IChunkClaims public immutable chunkClaims;
  uint96 public feeBps;
  uint256 public nextListingId = 1;
  uint256 public nextAuctionId = 1;

  mapping(uint256 => Listing) public listings;
  mapping(uint256 => Auction) public auctions;
  // We keep direct token -> active sale ids so reads stay cheap.
  mapping(uint256 => uint256) public activeListingByTokenId;
  mapping(uint256 => uint256) public activeAuctionByTokenId;

  event FeeBpsUpdated(uint96 feeBps);
  event ListingCreated(
    uint256 indexed listingId,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    address seller,
    int32 x,
    int32 y,
    uint256 price
  );
  event ListingCancelled(uint256 indexed listingId, uint256 indexed tokenId, bytes32 indexed chunkKey, string reason);
  event ListingPurchased(
    uint256 indexed listingId,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    address seller,
    address buyer,
    int32 x,
    int32 y,
    uint256 price,
    uint256 feePaid
  );
  event AuctionCreated(
    uint256 indexed auctionId,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    address seller,
    int32 x,
    int32 y,
    uint256 reservePrice,
    uint256 minBidIncrement,
    uint64 endTime
  );
  event AuctionBidPlaced(
    uint256 indexed auctionId,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    address bidder,
    uint256 amount,
    uint64 endTime
  );
  event AuctionCancelled(uint256 indexed auctionId, uint256 indexed tokenId, bytes32 indexed chunkKey, string reason);
  event AuctionSettled(
    uint256 indexed auctionId,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    address seller,
    address winner,
    int32 x,
    int32 y,
    uint256 finalBid,
    uint256 feePaid
  );

  constructor(address initialOwner, address chunkClaimsAddress, uint96 initialFeeBps) Ownable(initialOwner)
  {
    if (initialFeeBps > MAX_FEE_BPS)
    {
      revert InvalidFeeBps(initialFeeBps);
    }

    chunkClaims = IChunkClaims(chunkClaimsAddress);
    feeBps = initialFeeBps;
  }

  function setFeeBps(uint96 newFeeBps) external onlyOwner
  {
    if (newFeeBps > MAX_FEE_BPS)
    {
      revert InvalidFeeBps(newFeeBps);
    }

    feeBps = newFeeBps;
    emit FeeBpsUpdated(newFeeBps);
  }

  function createListing(int32 x, int32 y, uint256 price) external returns (uint256 listingId)
  {
    if (price == 0)
    {
      revert InvalidPrice(price);
    }

    uint256 tokenId = _requireSellerOwnsChunk(msg.sender, x, y, 0);
    _requireNoActiveSale(tokenId);

    listingId = nextListingId;
    nextListingId += 1;

    listings[listingId] = Listing({
      id: listingId,
      tokenId: tokenId,
      x: x,
      y: y,
      seller: msg.sender,
      price: price,
      active: true
    });
    activeListingByTokenId[tokenId] = listingId;

    emit ListingCreated(listingId, tokenId, _chunkKey(x, y), msg.sender, x, y, price);
  }

  function cancelListing(uint256 listingId) external
  {
    Listing storage listing = _requireActiveListing(listingId);
    if (listing.seller != msg.sender)
    {
      revert NotListingSeller(msg.sender, listingId);
    }

    _deactivateListing(listing);
    emit ListingCancelled(listingId, listing.tokenId, _chunkKey(listing.x, listing.y), "SELLER_CANCELLED");
  }

  function invalidateStaleListing(uint256 listingId) external
  {
    Listing storage listing = _requireActiveListing(listingId);
    if (chunkClaims.ownerOf(listing.x, listing.y) == listing.seller)
    {
      revert SellerNoLongerOwnsChunk(0);
    }

    _deactivateListing(listing);
    emit ListingCancelled(listingId, listing.tokenId, _chunkKey(listing.x, listing.y), "STALE_OWNER");
  }

  function purchaseListing(uint256 listingId) external payable
  {
    Listing storage listing = _requireActiveListing(listingId);
    if (!chunkClaims.isRegisteredPlayer(msg.sender))
    {
      revert NotRegisteredBuyer(msg.sender);
    }
    if (msg.value != listing.price)
    {
      revert IncorrectPayment(listing.price, msg.value);
    }

    _requireSellerOwnsChunk(listing.seller, listing.x, listing.y, listingId);
    _deactivateListing(listing);

    // The marketplace keeps a protocol fee and forwards the rest to the seller.
    uint256 feePaid = (listing.price * feeBps) / 10_000;
    uint256 sellerPayout = listing.price - feePaid;

    chunkClaims.marketplaceTransferChunk(listing.x, listing.y, listing.seller, msg.sender);

    (bool payoutOk, ) = payable(listing.seller).call{value: sellerPayout}("");
    if (!payoutOk)
    {
      revert SellerPayoutFailed(listing.seller, sellerPayout);
    }

    emit ListingPurchased(
      listingId,
      listing.tokenId,
      _chunkKey(listing.x, listing.y),
      listing.seller,
      msg.sender,
      listing.x,
      listing.y,
      listing.price,
      feePaid
    );
  }

  function createAuction(
    int32 x,
    int32 y,
    uint256 reservePrice,
    uint256 minBidIncrement,
    uint64 durationSeconds
  )
    external
    returns (uint256 auctionId)
  {
    if (reservePrice == 0)
    {
      revert InvalidPrice(reservePrice);
    }
    if (durationSeconds < MIN_AUCTION_DURATION)
    {
      revert InvalidAuctionDuration(durationSeconds);
    }

    uint256 tokenId = _requireSellerOwnsChunk(msg.sender, x, y, 0);
    _requireNoActiveSale(tokenId);

    auctionId = nextAuctionId;
    nextAuctionId += 1;

    uint64 endTime = uint64(block.timestamp) + durationSeconds;
    auctions[auctionId] = Auction({
      id: auctionId,
      tokenId: tokenId,
      x: x,
      y: y,
      seller: msg.sender,
      reservePrice: reservePrice,
      minBidIncrement: minBidIncrement,
      endTime: endTime,
      highestBidder: address(0),
      highestBid: 0,
      active: true,
      settled: false
    });
    activeAuctionByTokenId[tokenId] = auctionId;

    emit AuctionCreated(auctionId, tokenId, _chunkKey(x, y), msg.sender, x, y, reservePrice, minBidIncrement, endTime);
  }

  function placeAuctionBid(uint256 auctionId) external payable
  {
    Auction storage auction = _requireActiveAuction(auctionId);
    if (!chunkClaims.isRegisteredPlayer(msg.sender))
    {
      revert NotRegisteredBuyer(msg.sender);
    }
    if (block.timestamp >= auction.endTime)
    {
      revert AuctionAlreadyEnded(auctionId);
    }

    // First bid must meet the reserve; later bids must beat the current one by the increment.
    uint256 minimumBid = auction.highestBid == 0
      ? auction.reservePrice
      : auction.highestBid + auction.minBidIncrement;
    if (msg.value < minimumBid)
    {
      revert BidTooLow(minimumBid, msg.value);
    }

    address previousBidder = auction.highestBidder;
    uint256 previousBid = auction.highestBid;

    auction.highestBidder = msg.sender;
    auction.highestBid = msg.value;

    if (previousBidder != address(0))
    {
      (bool refundOk, ) = payable(previousBidder).call{value: previousBid}("");
      if (!refundOk)
      {
        revert BidRefundFailed(previousBidder, previousBid);
      }
    }

    emit AuctionBidPlaced(auctionId, auction.tokenId, _chunkKey(auction.x, auction.y), msg.sender, msg.value, auction.endTime);
  }

  function cancelAuction(uint256 auctionId) external
  {
    Auction storage auction = _requireActiveAuction(auctionId);
    if (auction.seller != msg.sender)
    {
      revert NotAuctionSeller(msg.sender, auctionId);
    }
    if (auction.highestBidder != address(0))
    {
      revert AuctionHasExistingBids(auctionId);
    }

    _deactivateAuction(auction);
    emit AuctionCancelled(auctionId, auction.tokenId, _chunkKey(auction.x, auction.y), "SELLER_CANCELLED");
  }

  function invalidateStaleAuction(uint256 auctionId) external
  {
    Auction storage auction = _requireActiveAuction(auctionId);
    if (chunkClaims.ownerOf(auction.x, auction.y) == auction.seller)
    {
      revert SellerNoLongerOwnsChunk(0);
    }

    address previousBidder = auction.highestBidder;
    uint256 previousBid = auction.highestBid;
    _deactivateAuction(auction);

    if (previousBidder != address(0))
    {
      (bool refundOk, ) = payable(previousBidder).call{value: previousBid}("");
      if (!refundOk)
      {
        revert BidRefundFailed(previousBidder, previousBid);
      }
    }

    emit AuctionCancelled(auctionId, auction.tokenId, _chunkKey(auction.x, auction.y), "STALE_OWNER");
  }

  function settleAuction(uint256 auctionId) external
  {
    Auction storage auction = _requireActiveAuction(auctionId);
    if (block.timestamp < auction.endTime)
    {
      revert AuctionNotEnded(auctionId);
    }

    _requireSellerOwnsChunk(auction.seller, auction.x, auction.y, auctionId);
    _deactivateAuction(auction);
    auction.settled = true;

    if (auction.highestBidder == address(0))
    {
      emit AuctionCancelled(auctionId, auction.tokenId, _chunkKey(auction.x, auction.y), "NO_BIDS");
      return;
    }

    uint256 feePaid = (auction.highestBid * feeBps) / 10_000;
    uint256 sellerPayout = auction.highestBid - feePaid;

    chunkClaims.marketplaceTransferChunk(auction.x, auction.y, auction.seller, auction.highestBidder);

    (bool payoutOk, ) = payable(auction.seller).call{value: sellerPayout}("");
    if (!payoutOk)
    {
      revert SellerPayoutFailed(auction.seller, sellerPayout);
    }

    emit AuctionSettled(
      auctionId,
      auction.tokenId,
      _chunkKey(auction.x, auction.y),
      auction.seller,
      auction.highestBidder,
      auction.x,
      auction.y,
      auction.highestBid,
      feePaid
    );
  }

  function getSaleStateForChunk(int32 x, int32 y) external view returns (SaleState memory)
  {
    return _getSaleStateForTokenAndChunk(chunkClaims.tokenIdOfChunk(x, y), x, y);
  }

  function getSaleStateForToken(uint256 tokenId) external view returns (SaleState memory)
  {
    Listing storage listing = listings[activeListingByTokenId[tokenId]];
    if (listing.active)
    {
      return _buildListingSaleState(listing);
    }

    Auction storage auction = auctions[activeAuctionByTokenId[tokenId]];
    if (auction.active)
    {
      return _buildAuctionSaleState(auction);
    }

    return SaleState({
      saleKind: SALE_KIND_NONE,
      saleId: 0,
      tokenId: tokenId,
      x: 0,
      y: 0,
      seller: address(0),
      sellerStillOwnsChunk: false,
      active: false,
      price: 0,
      reservePrice: 0,
      minBidIncrement: 0,
      endTime: 0,
      highestBidder: address(0),
      highestBid: 0
    });
  }

  function withdrawFees(address payable recipient) external onlyOwner
  {
    uint256 amount = address(this).balance;
    (bool ok, ) = recipient.call{value: amount}("");
    if (!ok)
    {
      revert FeeWithdrawalFailed(recipient, amount);
    }
  }

  function _requireActiveListing(uint256 listingId) private view returns (Listing storage listing)
  {
    listing = listings[listingId];
    if (!listing.active)
    {
      revert ListingNotActive(listingId);
    }
  }

  function _requireActiveAuction(uint256 auctionId) private view returns (Auction storage auction)
  {
    auction = auctions[auctionId];
    if (!auction.active)
    {
      revert AuctionNotActive(auctionId);
    }
  }

  function _requireSellerOwnsChunk(address seller, int32 x, int32 y, uint256 saleId) private view returns (uint256 tokenId)
  {
    if (chunkClaims.ownerOf(x, y) != seller)
    {
      revert SellerNoLongerOwnsChunk(saleId);
    }

    tokenId = chunkClaims.tokenIdOfChunk(x, y);
  }

  function _requireNoActiveSale(uint256 tokenId) private view
  {
    if (_isListingActive(activeListingByTokenId[tokenId]) || _isAuctionActive(activeAuctionByTokenId[tokenId]))
    {
      revert SaleAlreadyExists(tokenId);
    }
  }

  function _deactivateListing(Listing storage listing) private
  {
    listing.active = false;
    activeListingByTokenId[listing.tokenId] = 0;
  }

  function _deactivateAuction(Auction storage auction) private
  {
    auction.active = false;
    activeAuctionByTokenId[auction.tokenId] = 0;
  }

  function _isListingActive(uint256 listingId) private view returns (bool)
  {
    return listingId != 0 && listings[listingId].active;
  }

  function _isAuctionActive(uint256 auctionId) private view returns (bool)
  {
    return auctionId != 0 && auctions[auctionId].active;
  }

  function _getSaleStateForTokenAndChunk(uint256 tokenId, int32 x, int32 y) private view returns (SaleState memory)
  {
    Listing storage listing = listings[activeListingByTokenId[tokenId]];
    if (listing.active)
    {
      return _buildListingSaleState(listing);
    }

    Auction storage auction = auctions[activeAuctionByTokenId[tokenId]];
    if (auction.active)
    {
      return _buildAuctionSaleState(auction);
    }

    return SaleState({
      saleKind: SALE_KIND_NONE,
      saleId: 0,
      tokenId: tokenId,
      x: x,
      y: y,
      seller: address(0),
      sellerStillOwnsChunk: false,
      active: false,
      price: 0,
      reservePrice: 0,
      minBidIncrement: 0,
      endTime: 0,
      highestBidder: address(0),
      highestBid: 0
    });
  }

  function _buildListingSaleState(Listing storage listing) private view returns (SaleState memory)
  {
    return SaleState({
      saleKind: SALE_KIND_LISTING,
      saleId: listing.id,
      tokenId: listing.tokenId,
      x: listing.x,
      y: listing.y,
      seller: listing.seller,
      sellerStillOwnsChunk: chunkClaims.ownerOf(listing.x, listing.y) == listing.seller,
      active: listing.active,
      price: listing.price,
      reservePrice: 0,
      minBidIncrement: 0,
      endTime: 0,
      highestBidder: address(0),
      highestBid: 0
    });
  }

  function _buildAuctionSaleState(Auction storage auction) private view returns (SaleState memory)
  {
    return SaleState({
      saleKind: SALE_KIND_AUCTION,
      saleId: auction.id,
      tokenId: auction.tokenId,
      x: auction.x,
      y: auction.y,
      seller: auction.seller,
      sellerStillOwnsChunk: chunkClaims.ownerOf(auction.x, auction.y) == auction.seller,
      active: auction.active,
      price: 0,
      reservePrice: auction.reservePrice,
      minBidIncrement: auction.minBidIncrement,
      endTime: auction.endTime,
      highestBidder: auction.highestBidder,
      highestBid: auction.highestBid
    });
  }

  function _chunkKey(int32 x, int32 y) private pure returns (bytes32)
  {
    return keccak256(abi.encodePacked(x, y));
  }
}
