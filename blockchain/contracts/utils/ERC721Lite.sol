// SPDX-License-Identifier: MIT
pragma solidity ^0.8.24;

error ERC721InvalidOwner(address owner);
error ERC721InvalidReceiver(address receiver);
error ERC721NonexistentToken(uint256 tokenId);
error ERC721IncorrectOwner(address from, uint256 tokenId, address actualOwner);
error ERC721InsufficientApproval(address operator, uint256 tokenId);
error ERC721InvalidApprover(address approver);
error ERC721InvalidOperator(address operator);

contract ERC721Lite
{
  string public name;
  string public symbol;

  mapping(uint256 => address) private tokenOwners;
  mapping(address => uint256) private ownerBalances;
  mapping(uint256 => address) private tokenApprovals;
  mapping(address => mapping(address => bool)) private operatorApprovals;

  event Transfer(address indexed from, address indexed to, uint256 indexed tokenId);
  event Approval(address indexed owner, address indexed approved, uint256 indexed tokenId);
  event ApprovalForAll(address indexed owner, address indexed operator, bool approved);

  constructor(string memory tokenName, string memory tokenSymbol)
  {
    name = tokenName;
    symbol = tokenSymbol;
  }

  function balanceOf(address owner) public view returns (uint256)
  {
    if (owner == address(0))
    {
      revert ERC721InvalidOwner(owner);
    }

    return ownerBalances[owner];
  }

  function ownerOfToken(uint256 tokenId) public view returns (address)
  {
    address owner = tokenOwners[tokenId];
    if (owner == address(0))
    {
      revert ERC721NonexistentToken(tokenId);
    }

    return owner;
  }

  function getApproved(uint256 tokenId) public view returns (address)
  {
    _requireOwned(tokenId);
    return tokenApprovals[tokenId];
  }

  function isApprovedForAll(address owner, address operator) public view returns (bool)
  {
    return operatorApprovals[owner][operator];
  }

  function approve(address to, uint256 tokenId) external
  {
    address owner = ownerOfToken(tokenId);
    if (to == owner)
    {
      revert ERC721InvalidOperator(to);
    }
    if (msg.sender != owner && !operatorApprovals[owner][msg.sender])
    {
      revert ERC721InvalidApprover(msg.sender);
    }

    tokenApprovals[tokenId] = to;
    emit Approval(owner, to, tokenId);
  }

  function setApprovalForAll(address operator, bool approved) external
  {
    if (operator == msg.sender)
    {
      revert ERC721InvalidOperator(operator);
    }

    operatorApprovals[msg.sender][operator] = approved;
    emit ApprovalForAll(msg.sender, operator, approved);
  }

  function transferFrom(address from, address to, uint256 tokenId) public
  {
    if (!_isAuthorized(msg.sender, tokenId))
    {
      revert ERC721InsufficientApproval(msg.sender, tokenId);
    }

    _transfer(from, to, tokenId);
  }

  function tokenURI(uint256 tokenId) public view virtual returns (string memory)
  {
    _requireOwned(tokenId);
    return "";
  }

  function _mint(address to, uint256 tokenId) internal
  {
    if (to == address(0))
    {
      revert ERC721InvalidReceiver(to);
    }
    if (tokenOwners[tokenId] != address(0))
    {
      revert ERC721InvalidOperator(to);
    }

    _beforeTokenTransfer(address(0), to, tokenId);
    ownerBalances[to] += 1;
    tokenOwners[tokenId] = to;
    emit Transfer(address(0), to, tokenId);
    _afterTokenTransfer(address(0), to, tokenId);
  }

  function _burn(uint256 tokenId) internal
  {
    address owner = ownerOfToken(tokenId);

    _beforeTokenTransfer(owner, address(0), tokenId);
    delete tokenApprovals[tokenId];
    ownerBalances[owner] -= 1;
    delete tokenOwners[tokenId];
    emit Transfer(owner, address(0), tokenId);
    _afterTokenTransfer(owner, address(0), tokenId);
  }

  function _transfer(address from, address to, uint256 tokenId) internal
  {
    if (to == address(0))
    {
      revert ERC721InvalidReceiver(to);
    }

    address owner = ownerOfToken(tokenId);
    if (owner != from)
    {
      revert ERC721IncorrectOwner(from, tokenId, owner);
    }

    _beforeTokenTransfer(from, to, tokenId);
    delete tokenApprovals[tokenId];
    ownerBalances[from] -= 1;
    ownerBalances[to] += 1;
    tokenOwners[tokenId] = to;
    emit Transfer(from, to, tokenId);
    _afterTokenTransfer(from, to, tokenId);
  }

  function _isAuthorized(address operator, uint256 tokenId) internal view returns (bool)
  {
    address owner = ownerOfToken(tokenId);
    return operator == owner || tokenApprovals[tokenId] == operator || operatorApprovals[owner][operator];
  }

  function _requireOwned(uint256 tokenId) internal view returns (address)
  {
    return ownerOfToken(tokenId);
  }

  function _beforeTokenTransfer(address from, address to, uint256 tokenId) internal virtual
  {
    from;
    to;
    tokenId;
  }

  function _afterTokenTransfer(address from, address to, uint256 tokenId) internal virtual
  {
    from;
    to;
    tokenId;
  }
}
