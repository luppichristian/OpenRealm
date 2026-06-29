// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import "./interfaces/IPlayerRegistry.sol";
import "./interfaces/IGlobalParams.sol";
import "./utils/ERC721Lite.sol";
import "./utils/Ownable.sol";

error InvalidChunkCoordinate(int32 x, int32 y);
error ChunkAlreadyClaimed(int32 x, int32 y);
error ChunkNotClaimed(int32 x, int32 y);
error NotChunkOwner(address account, int32 x, int32 y);
error NotRegisteredPlayer(address account);
error InvalidMarketplace(address marketplace);
error InvalidGlobalParams(address globalParams);
error UnauthorizedMarketplace(address account);
error CannotDelegateToZeroAddress();
error UnknownChunkToken(uint256 tokenId);
error IncorrectChunkPurchasePrice(uint256 expected, uint256 actual);
error ChunkRefundFailed(address recipient, uint256 amount);

contract ChunkClaims is Ownable, ERC721Lite
{
  // Minimal on-chain data for one claimed chunk.
  struct ChunkClaim
  {
    uint256 tokenId;
    int32 x;
    int32 y;
    uint64 claimedAt;
  }

  // One-call view tailored for runtime permission checks.
  struct RuntimeChunkState
  {
    bool claimed;
    uint256 tokenId;
    int32 x;
    int32 y;
    address owner;
    uint256 ownerPlayerId;
    uint64 claimedAt;
    uint64 editorEpoch;
    address resolvedActor;
    bool actorRegistered;
    bool actorCanEdit;
    bool actorUsesRuntimeSession;
  }

  IPlayerRegistry public immutable registry;
  IGlobalParams public immutable globalParams;
  address public marketplace;
  uint256 public nextTokenId = 1;

  // chunk key -> NFT token id.
  mapping(bytes32 => uint256) private chunkTokenIds;
  mapping(uint256 => ChunkClaim) private claimsByTokenId;
  mapping(uint256 => uint256) private claimPricesByTokenId;
  // Incremented whenever cached permission data should be considered stale.
  mapping(uint256 => uint64) private editorEpochs;
  // token id -> permission epoch -> editor wallet -> allowed.
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

  constructor(address initialOwner, address registryAddress, address globalParamsAddress)
    Ownable(initialOwner)
    ERC721Lite("OpenRealm Chunk Claim", "ORCHUNK")
  {
    if (registryAddress == address(0))
    {
      revert NotRegisteredPlayer(address(0));
    }
    if (globalParamsAddress == address(0))
    {
      revert InvalidGlobalParams(globalParamsAddress);
    }

    registry = IPlayerRegistry(registryAddress);
    globalParams = IGlobalParams(globalParamsAddress);
  }

  function SetMarketplace(address marketplaceAddress) external onlyOwner
  {
    if (marketplaceAddress == address(0))
    {
      revert InvalidMarketplace(marketplaceAddress);
    }

    marketplace = marketplaceAddress;
    emit MarketplaceUpdated(marketplaceAddress);
  }

  function ClaimChunk(int32 x, int32 y) external payable returns (uint256 tokenId)
  {
    _requireRegistered(msg.sender);
    _requireValidChunk(x, y);

    uint256 claimPrice = globalParams.MIN_CHUNK_PRICE();
    if (msg.value != claimPrice)
    {
      revert IncorrectChunkPurchasePrice(claimPrice, msg.value);
    }

    // The chunk key is the canonical hash used for ownership lookups.
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
    claimPricesByTokenId[tokenId] = claimPrice;
    _mint(msg.sender, tokenId);

    emit ChunkClaimed(msg.sender, tokenId, chunkKey, x, y, claimedAt);
  }

  function AbandonChunk(int32 x, int32 y) external
  {
    bytes32 chunkKey = _requireChunkOwner(msg.sender, x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    address previousOwner = ownerOfToken(tokenId);
    uint256 claimPrice = claimPricesByTokenId[tokenId];

    delete chunkTokenIds[chunkKey];
    delete claimsByTokenId[tokenId];
    delete claimPricesByTokenId[tokenId];
    editorEpochs[tokenId] += 1;
    _burn(tokenId);

    (bool refundOk, ) = payable(previousOwner).call{value: claimPrice}("");
    if (!refundOk)
    {
      revert ChunkRefundFailed(previousOwner, claimPrice);
    }

    emit ChunkAbandoned(previousOwner, tokenId, chunkKey, x, y);
  }

  function TransferChunk(int32 x, int32 y, address newOwner) external
  {
    bytes32 chunkKey = _requireChunkOwner(msg.sender, x, y);
    _requireRegistered(newOwner);
    _transfer(msg.sender, newOwner, chunkTokenIds[chunkKey]);
  }

  function SetChunkEditor(int32 x, int32 y, address editor, bool allowed) external
  {
    if (editor == address(0))
    {
      revert CannotDelegateToZeroAddress();
    }

    bytes32 chunkKey = _requireChunkOwner(msg.sender, x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    // Permissions live inside the current epoch so a transfer can invalidate them cheaply.
    uint64 editorEpoch = editorEpochs[tokenId];
    delegatedEditors[tokenId][editorEpoch][editor] = allowed;

    emit ChunkEditorUpdated(tokenId, chunkKey, editor, x, y, allowed, editorEpoch);
  }

  function MarketplaceTransferChunk(int32 x, int32 y, address from, address to) external
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

  function OwnerOf(int32 x, int32 y) external view returns (address)
  {
    uint256 tokenId = chunkTokenIds[_chunkKey(x, y)];
    if (tokenId == 0)
    {
      return address(0);
    }

    return ownerOfToken(tokenId);
  }

  function TokenIdOfChunk(int32 x, int32 y) external view returns (uint256)
  {
    return chunkTokenIds[_chunkKey(x, y)];
  }

  function GetClaim(int32 x, int32 y) external view returns (ChunkClaim memory)
  {
    uint256 tokenId = chunkTokenIds[_chunkKey(x, y)];
    if (tokenId == 0)
    {
      return ChunkClaim({tokenId: 0, x: 0, y: 0, claimedAt: 0});
    }

    return claimsByTokenId[tokenId];
  }

  function GetClaimByTokenId(uint256 tokenId) external view returns (ChunkClaim memory)
  {
    ChunkClaim memory claim = claimsByTokenId[tokenId];
    if (claim.tokenId == 0)
    {
      revert UnknownChunkToken(tokenId);
    }

    return claim;
  }

  function CanEdit(int32 x, int32 y, address account) public view returns (bool)
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

  function CanEditWithRuntimeSigner(
    int32 x,
    int32 y,
    address actor
  )
    external
    view
    returns (bool allowed, address resolvedActor, bool actorUsesRuntimeSession)
  {
    // Resolve the incoming signer first, because runtime code may use a delegated session key.
    (resolvedActor, , actorUsesRuntimeSession) = registry.ResolveRuntimeAccount(actor);
    if (resolvedActor == address(0))
    {
      return (false, address(0), actorUsesRuntimeSession);
    }

    return (CanEdit(x, y, resolvedActor), resolvedActor, actorUsesRuntimeSession);
  }

  function EditorEpochOfChunk(int32 x, int32 y) external view returns (uint64)
  {
    uint256 tokenId = chunkTokenIds[_chunkKey(x, y)];
    if (tokenId == 0)
    {
      return 0;
    }

    return editorEpochs[tokenId];
  }

  function GetChunkRuntimeState(int32 x, int32 y, address actor) external view returns (RuntimeChunkState memory state)
  {
    _requireValidChunk(x, y);

    bytes32 chunkKey = _chunkKey(x, y);
    uint256 tokenId = chunkTokenIds[chunkKey];
    // The runtime may pass either a wallet or a temporary session signer here.
    (address resolvedActor, , bool actorUsesRuntimeSession) = registry.ResolveRuntimeAccount(actor);

    state.x = x;
    state.y = y;
    state.resolvedActor = resolvedActor;
    state.actorRegistered = resolvedActor != address(0);
    state.actorUsesRuntimeSession = actorUsesRuntimeSession;

    if (tokenId == 0)
    {
      return state;
    }

    ChunkClaim memory claim = claimsByTokenId[tokenId];
    address chunkOwner = ownerOfToken(tokenId);
    uint64 editorEpoch = editorEpochs[tokenId];

    state.claimed = true;
    state.tokenId = tokenId;
    state.owner = chunkOwner;
    state.ownerPlayerId = registry.PlayerIdOf(chunkOwner);
    state.claimedAt = claim.claimedAt;
    state.editorEpoch = editorEpoch;
    // This is the core permission answer the runtime cares about for edit authorization.
    state.actorCanEdit = resolvedActor != address(0) && (
      chunkOwner == resolvedActor || delegatedEditors[tokenId][editorEpoch][resolvedActor]
    );
  }

  function IsRegisteredPlayer(address account) external view returns (bool)
  {
    return registry.IsRegistered(account);
  }

  function EncodeChunkKey(int32 x, int32 y) external pure returns (bytes32)
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
      // Any ownership change invalidates cached delegated-editor permissions.
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

  function _requireValidChunk(int32 x, int32 y) private view
  {
    int32 minChunkCoord = globalParams.MIN_CHUNK_COORD();
    int32 maxChunkCoord = globalParams.MAX_CHUNK_COORD();
    if (x < minChunkCoord || x > maxChunkCoord || y < minChunkCoord || y > maxChunkCoord)
    {
      revert InvalidChunkCoordinate(x, y);
    }
  }

  function _requireRegistered(address account) private view
  {
    if (!registry.IsRegistered(account))
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
