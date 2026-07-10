// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import "./utils/Ownable.sol";

error EmptyHandle();
error AlreadyRegistered(address account);
error HandleAlreadyTaken(string handle);
error ProfileNotRegistered(address account);
error InvalidRuntimeSession(address session);
error InvalidRuntimeSessionExpiry(uint64 expiresAt);
error RuntimeSessionAlreadyBound(address session, address account);
error RuntimeSessionNotOwnedByAccount(address account, address session);

contract PlayerRegistry is Ownable
{
  // One long-lived on-chain player identity per wallet.
  struct PlayerProfile
  {
    uint256 playerId;
    string handle;
    string metadataURI;
    bool active;
  }

  struct RuntimeSession
  {
    // Wallet account that authorized this temporary runtime signer.
    address account;
    uint64 expiresAt;
    bool active;
  }

  uint256 public nextPlayerId = 1;

  mapping(address => PlayerProfile) private profiles;
  // Handle lookups use a normalized keccak256 hash so uniqueness checks stay cheap.
  mapping(bytes32 => address) private handleOwners;
  // Runtime session key -> wallet account that authorized it.
  mapping(address => RuntimeSession) private runtimeSessions;

  event PlayerRegistered(
    address indexed account,
    uint256 indexed playerId,
    bytes32 indexed handleHash,
    string handle,
    string metadataURI
  );
  event PlayerUnregistered(address indexed account, uint256 indexed playerId, bytes32 indexed handleHash, string handle);
  event PlayerMetadataUpdated(address indexed account, uint256 indexed playerId, string metadataURI);
  event PlayerHandleUpdated(
    address indexed account,
    uint256 indexed playerId,
    bytes32 indexed newHandleHash,
    string newHandle
  );
  event RuntimeSessionAuthorized(address indexed account, address indexed session, uint64 expiresAt);
  event RuntimeSessionRevoked(address indexed account, address indexed session);

  constructor(address initialOwner) Ownable(initialOwner)
  {
  }

  function Register(string calldata handle, string calldata metadataURI) external
  {
    PlayerProfile storage profile = profiles[msg.sender];

    if (profile.active)
    {
      revert AlreadyRegistered(msg.sender);
    }

    _requireValidHandle(handle);

    bytes32 handleHash = _handleHash(handle);
    address existingOwner = handleOwners[handleHash];
    if (existingOwner != address(0))
    {
      revert HandleAlreadyTaken(handle);
    }

    // Reuse the old player id if this wallet registered before, otherwise mint a new one.
    uint256 playerId = profile.playerId;
    if (playerId == 0)
    {
      playerId = nextPlayerId;
      nextPlayerId += 1;
    }

    profile.playerId = playerId;
    profile.handle = handle;
    profile.metadataURI = metadataURI;
    profile.active = true;
    handleOwners[handleHash] = msg.sender;

    emit PlayerRegistered(msg.sender, playerId, handleHash, handle, metadataURI);
  }

  function Unregister() external
  {
    PlayerProfile storage profile = _requireActiveProfile(msg.sender);
    bytes32 handleHash = _handleHash(profile.handle);
    handleOwners[handleHash] = address(0);
    profile.active = false;

    emit PlayerUnregistered(msg.sender, profile.playerId, handleHash, profile.handle);
  }

  function UpdateMetadataURI(string calldata metadataURI) external
  {
    PlayerProfile storage profile = _requireActiveProfile(msg.sender);
    profile.metadataURI = metadataURI;

    emit PlayerMetadataUpdated(msg.sender, profile.playerId, metadataURI);
  }

  function UpdateHandle(string calldata newHandle) external
  {
    PlayerProfile storage profile = _requireActiveProfile(msg.sender);
    _requireValidHandle(newHandle);

    bytes32 newHandleHash = _handleHash(newHandle);
    address existingOwner = handleOwners[newHandleHash];
    if (existingOwner != address(0) && existingOwner != msg.sender)
    {
      revert HandleAlreadyTaken(newHandle);
    }

    bytes32 previousHandleHash = _handleHash(profile.handle);
    handleOwners[previousHandleHash] = address(0);
    handleOwners[newHandleHash] = msg.sender;
    profile.handle = newHandle;

    emit PlayerHandleUpdated(msg.sender, profile.playerId, newHandleHash, newHandle);
  }

  function AuthorizeRuntimeSession(address session, uint64 expiresAt) external
  {
    _requireActiveProfile(msg.sender);

    if (session == address(0) || session == msg.sender)
    {
      revert InvalidRuntimeSession(session);
    }
    if (expiresAt <= block.timestamp)
    {
      revert InvalidRuntimeSessionExpiry(expiresAt);
    }

    // A live session key can only belong to one wallet at a time.
    RuntimeSession storage existingSession = runtimeSessions[session];
    if (existingSession.active && existingSession.expiresAt >= block.timestamp && existingSession.account != msg.sender)
    {
      revert RuntimeSessionAlreadyBound(session, existingSession.account);
    }

    runtimeSessions[session] = RuntimeSession({account: msg.sender, expiresAt: expiresAt, active: true});
    emit RuntimeSessionAuthorized(msg.sender, session, expiresAt);
  }

  function RevokeRuntimeSession(address session) external
  {
    RuntimeSession storage runtimeSession = runtimeSessions[session];
    if (!runtimeSession.active || runtimeSession.account != msg.sender)
    {
      revert RuntimeSessionNotOwnedByAccount(msg.sender, session);
    }

    delete runtimeSessions[session];
    emit RuntimeSessionRevoked(msg.sender, session);
  }

  function GetProfile(
    address account
  )
    external
    view
    returns (uint256 playerId, string memory handle, string memory metadataURI, bool active)
  {
    PlayerProfile storage profile = profiles[account];
    return (profile.playerId, profile.handle, profile.metadataURI, profile.active);
  }

  function GetRuntimeSession(address session) external view returns (address account, uint64 expiresAt, bool active)
  {
    RuntimeSession storage runtimeSession = runtimeSessions[session];
    return (runtimeSession.account, runtimeSession.expiresAt, runtimeSession.active);
  }

  function PlayerIdOf(address account) external view returns (uint256)
  {
    return profiles[account].playerId;
  }

  function IsRegistered(address account) external view returns (bool)
  {
    return profiles[account].active;
  }

  function AccountForHandle(string calldata handle) external view returns (address)
  {
    return handleOwners[_handleHash(handle)];
  }

  function NormalizedHandleHash(string calldata handle) external pure returns (bytes32)
  {
    return _handleHash(handle);
  }

  function HandleHashAvailable(bytes32 handleHash) external view returns (bool)
  {
    return handleOwners[handleHash] == address(0);
  }

  function ResolveRuntimeAccount(address actor) external view returns (address account, bool isDirect, bool isRuntimeSession)
  {
    // First treat the signer as a normal registered wallet address.
    PlayerProfile storage directProfile = profiles[actor];
    if (directProfile.active)
    {
      return (actor, true, false);
    }

    // If it is not a wallet, try resolving it as a delegated runtime session key.
    RuntimeSession storage runtimeSession = runtimeSessions[actor];
    if (
      runtimeSession.active && runtimeSession.expiresAt >= block.timestamp && profiles[runtimeSession.account].active
    )
    {
      return (runtimeSession.account, false, true);
    }

    return (address(0), false, false);
  }

  function _requireActiveProfile(address account) private view returns (PlayerProfile storage profile)
  {
    profile = profiles[account];
    if (!profile.active)
    {
      revert ProfileNotRegistered(account);
    }
  }

  function _requireValidHandle(string calldata handle) private pure
  {
    if (bytes(handle).length == 0)
    {
      revert EmptyHandle();
    }
  }

  function _handleHash(string memory handle) private pure returns (bytes32)
  {
    bytes memory normalized = bytes(handle);
    for (uint256 index = 0; index < normalized.length; index += 1)
    {
      bytes1 character = normalized[index];
      if (character >= 0x41 && character <= 0x5A)
      {
        normalized[index] = bytes1(uint8(character) + 32);
      }
    }

    return keccak256(normalized);
  }
}
