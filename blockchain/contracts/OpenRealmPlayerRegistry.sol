// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

import "./utils/Ownable.sol";

error EmptyHandle();
error AlreadyRegistered(address account);
error HandleAlreadyTaken(string handle);
error ProfileNotRegistered(address account);

contract OpenRealmPlayerRegistry is Ownable
{
  struct PlayerProfile
  {
    uint256 playerId;
    string handle;
    string metadataURI;
    bool active;
  }

  uint256 public nextPlayerId = 1;

  mapping(address => PlayerProfile) private profiles;
  mapping(bytes32 => address) private handleOwners;

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

  constructor(address initialOwner) Ownable(initialOwner)
  {
  }

  function register(string calldata handle, string calldata metadataURI) external
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

  function unregister() external
  {
    PlayerProfile storage profile = _requireActiveProfile(msg.sender);
    bytes32 handleHash = _handleHash(profile.handle);
    handleOwners[handleHash] = address(0);
    profile.active = false;

    emit PlayerUnregistered(msg.sender, profile.playerId, handleHash, profile.handle);
  }

  function updateMetadataURI(string calldata metadataURI) external
  {
    PlayerProfile storage profile = _requireActiveProfile(msg.sender);
    profile.metadataURI = metadataURI;

    emit PlayerMetadataUpdated(msg.sender, profile.playerId, metadataURI);
  }

  function updateHandle(string calldata newHandle) external
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

  function getProfile(
    address account
  )
    external
    view
    returns (uint256 playerId, string memory handle, string memory metadataURI, bool active)
  {
    PlayerProfile storage profile = profiles[account];
    return (profile.playerId, profile.handle, profile.metadataURI, profile.active);
  }

  function playerIdOf(address account) external view returns (uint256)
  {
    return profiles[account].playerId;
  }

  function isRegistered(address account) external view returns (bool)
  {
    return profiles[account].active;
  }

  function accountForHandle(string calldata handle) external view returns (address)
  {
    return handleOwners[_handleHash(handle)];
  }

  function handleHashAvailable(bytes32 handleHash) external view returns (bool)
  {
    return handleOwners[handleHash] == address(0);
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
    return keccak256(bytes(handle));
  }
}
