// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import "./interfaces/IOpenRealmPlayerRegistry.sol";
import "./utils/ERC721Lite.sol";
import "./utils/Ownable.sol";

error InvalidChunkCoordinate(int32 x, int32 y);
error ChunkAlreadyClaimed(int32 x, int32 y);
error ChunkNotClaimed(int32 x, int32 y);
error NotChunkOwner(address account, int32 x, int32 y);
error NotRegisteredPlayer(address account);
error InvalidMarketplace(address marketplace);
error UnauthorizedMarketplace(address account);
error CannotDelegateToZeroAddress();
error UnknownChunkToken(uint256 tokenId);

contract OpenRealmChunkClaims is Ownable, ERC721Lite
{
  struct ChunkClaim
  {
    uint256 tokenId;
    int32 x;
    int32 y;
    uint64 claimedAt;
  }

  int32 public constant MIN_CHUNK_COORD = -30000;
  int32 public constant MAX_CHUNK_COORD = 30000;

  IOpenRealmPlayerRegistry public immutable registry;
  address public marketplace;
  uint256 public nextTokenId = 1;

  mapping(bytes32 => uint256) private chunkTokenIds;
  mapping(uint256 => ChunkClaim) private claimsByTokenId;
  mapping(uint256 => uint64) private editorEpochs;
  mapping(uint256 => mapping(uint64 => mapping(address => bool))) private delegatedEditors;

  event MarketplaceUpdated(address indexed marketplace);
  event ChunkClaimed(
    address indexed owner,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    int32 x,
    int32 y,
    uint64 claimedAt
  );
  event ChunkTransferred(
    address indexed previousOwner,
    address indexed newOwner,
    uint256 indexed tokenId,
    bytes32 chunkKey,
    int32 x,
    int32 y,
    address operator
  );
  event ChunkAbandoned(
    address indexed previousOwner,
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    int32 x,
    int32 y
  );
  event ChunkEditorUpdated(
    uint256 indexed tokenId,
    bytes32 indexed chunkKey,
    address indexed editor,
    int32 x,
    int32 y,
    bool allowed,
    uint64 editorEpoch
  );

  constructor(address initialOwner, address registryAddress)
    Ownable(initialOwner)
    ERC721Lite("OpenRealm Chunk Claim", "ORCHUNK")
  {
    if (registryAddress == address(0))
    {
      revert NotRegisteredPlayer(address(0));
    }

    registry = IOpenRealmPlayerRegistry(registryAddress);
  }

  function setMarketplace(address marketplaceAddress) external onlyOwner
  {
    if (marketplaceAddress == address(0))
    {
      revert InvalidMarketplace(marketplaceAddress);
    }

    marketplace = marketplaceAddress;
    emit MarketplaceUpdated(marketplaceAddress);
  }

  function claimChunk(int32 x, int32 y) external returns (uint256 tokenId)
  {
    _requireRegistered(msg.sender);
    _requireValidChunk(x, y);

    bytes32 chunkKey = _chunkKey(x, y);
    if (chunkTokenIds[chunkKey] != 0)
    {
      revert ChunkAlreadyClaimed(x, y);
    }

    tokenId = nextTokenId;
    nextTokenId += 1;

    uint64 claimedAt = uint64(block.timestamp);
    chunkTokenIds[chunkKey] = tokenId;
    claimsByTokenId[tokenId] = ChunkClaim({tokenId: tokenId, x: x, y: y, claimedAt: claimedAt});
    _mint(msg.sender, tokenId);

    emit ChunkClaimed(msg.sender, tokenId, chunkKey, x, y, claimedAt);
  }

  function abandonChunk(int32 x, int32 y) external
  {
    bytes32 chunkKey = _requireChunkOwner(msg.sender, x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    address previousOwner = ownerOfToken(tokenId);

    delete chunkTokenIds[chunkKey];
    delete claimsByTokenId[tokenId];
    editorEpochs[tokenId] += 1;
    _burn(tokenId);

    emit ChunkAbandoned(previousOwner, tokenId, chunkKey, x, y);
  }

  function transferChunk(int32 x, int32 y, address newOwner) external
  {
    bytes32 chunkKey = _requireChunkOwner(msg.sender, x, y);
    _requireRegistered(newOwner);
    _transfer(msg.sender, newOwner, chunkTokenIds[chunkKey]);
  }

  function setChunkEditor(int32 x, int32 y, address editor, bool allowed) external
  {
    if (editor == address(0))
    {
      revert CannotDelegateToZeroAddress();
    }

    bytes32 chunkKey = _requireChunkOwner(msg.sender, x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    uint64 editorEpoch = editorEpochs[tokenId];
    delegatedEditors[tokenId][editorEpoch][editor] = allowed;

    emit ChunkEditorUpdated(tokenId, chunkKey, editor, x, y, allowed, editorEpoch);
  }

  function marketplaceTransferChunk(int32 x, int32 y, address from, address to) external
  {
    if (msg.sender != marketplace)
    {
      revert UnauthorizedMarketplace(msg.sender);
    }

    bytes32 chunkKey = _chunkKey(x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    if (tokenId == 0)
    {
      revert ChunkNotClaimed(x, y);
    }
    if (ownerOfToken(tokenId) != from)
    {
      revert NotChunkOwner(from, x, y);
    }

    _requireRegistered(to);
    _transfer(from, to, tokenId);
  }

  function ownerOf(int32 x, int32 y) external view returns (address)
  {
    uint256 tokenId = chunkTokenIds[_chunkKey(x, y)];
    if (tokenId == 0)
    {
      return address(0);
    }

    return ownerOfToken(tokenId);
  }

  function tokenIdOfChunk(int32 x, int32 y) external view returns (uint256)
  {
    return chunkTokenIds[_chunkKey(x, y)];
  }

  function getClaim(int32 x, int32 y) external view returns (ChunkClaim memory)
  {
    uint256 tokenId = chunkTokenIds[_chunkKey(x, y)];
    if (tokenId == 0)
    {
      return ChunkClaim({tokenId: 0, x: 0, y: 0, claimedAt: 0});
    }

    return claimsByTokenId[tokenId];
  }

  function getClaimByTokenId(uint256 tokenId) external view returns (ChunkClaim memory)
  {
    ChunkClaim memory claim = claimsByTokenId[tokenId];
    if (claim.tokenId == 0)
    {
      revert UnknownChunkToken(tokenId);
    }

    return claim;
  }

  function canEdit(int32 x, int32 y, address account) external view returns (bool)
  {
    uint256 tokenId = chunkTokenIds[_chunkKey(x, y)];
    if (tokenId == 0)
    {
      return false;
    }

    address chunkOwner = ownerOfToken(tokenId);
    uint64 editorEpoch = editorEpochs[tokenId];
    return chunkOwner == account || delegatedEditors[tokenId][editorEpoch][account];
  }

  function isRegisteredPlayer(address account) external view returns (bool)
  {
    return registry.isRegistered(account);
  }

  function encodeChunkKey(int32 x, int32 y) external pure returns (bytes32)
  {
    return _chunkKey(x, y);
  }

  function tokenURI(uint256 tokenId) public view override returns (string memory)
  {
    _requireOwned(tokenId);
    return "openrealm://chunk-claim";
  }

  function _beforeTokenTransfer(address from, address to, uint256 tokenId) internal override
  {
    if (to != address(0))
    {
      _requireRegistered(to);
    }

    if (from != address(0) && to != address(0))
    {
      editorEpochs[tokenId] += 1;
    }
  }

  function _afterTokenTransfer(address from, address to, uint256 tokenId) internal override
  {
    if (from == address(0) || to == address(0))
    {
      return;
    }

    ChunkClaim memory claim = claimsByTokenId[tokenId];
    emit ChunkTransferred(from, to, tokenId, _chunkKey(claim.x, claim.y), claim.x, claim.y, msg.sender);
  }

  function _requireValidChunk(int32 x, int32 y) private pure
  {
    if (x < MIN_CHUNK_COORD || x > MAX_CHUNK_COORD || y < MIN_CHUNK_COORD || y > MAX_CHUNK_COORD)
    {
      revert InvalidChunkCoordinate(x, y);
    }
  }

  function _requireRegistered(address account) private view
  {
    if (!registry.isRegistered(account))
    {
      revert NotRegisteredPlayer(account);
    }
  }

  function _requireChunkOwner(address account, int32 x, int32 y) private view returns (bytes32 chunkKey)
  {
    _requireValidChunk(x, y);
    chunkKey = _chunkKey(x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    if (tokenId == 0)
    {
      revert ChunkNotClaimed(x, y);
    }
    if (ownerOfToken(tokenId) != account)
    {
      revert NotChunkOwner(account, x, y);
    }
  }

  function _chunkKey(int32 x, int32 y) private pure returns (bytes32)
  {
    return keccak256(abi.encodePacked(x, y));
  }
}
