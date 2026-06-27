const assert = require('assert');
const ganache = require('ganache');
const { ethers } = require('ethers');
const { compileContracts, getContractArtifact } = require('./helpers/compileContracts');

describe('OpenRealm orchestration layer', function ()
{
  this.timeout(20000);

  let compiledContracts;

  async function deploy(contractName, signer, args = [])
  {
    const artifact = getContractArtifact(compiledContracts, `${contractName}.sol`, contractName);
    const factory = new ethers.ContractFactory(artifact.abi, artifact.bytecode, signer);
    const contract = await factory.deploy(...args);
    await contract.waitForDeployment();
    return contract;
  }

  async function expectRevert(action, expectedMessage)
  {
    let failed = false;

    try
    {
      const tx = await action();
      if (tx && typeof tx.wait === 'function')
      {
        await tx.wait();
      }
    }
    catch (error)
    {
      failed = true;
      if (expectedMessage)
      {
        const message = `${error.shortMessage || ''}\n${error.message || ''}`;
        assert.match(message, new RegExp(expectedMessage));
      }
    }

    if (!failed)
    {
      assert.fail(`Expected transaction to revert${expectedMessage ? ` with ${expectedMessage}` : ''}`);
    }
  }

  before(function ()
  {
    compiledContracts = compileContracts();
  });

  async function deployFixture()
  {
    const provider = new ethers.BrowserProvider(ganache.provider({ logging: { quiet: true } }));
    const owner = await provider.getSigner(0);
    const alice = await provider.getSigner(1);
    const bob = await provider.getSigner(2);
    const carol = await provider.getSigner(3);
    const dave = await provider.getSigner(4);

    const registry = await deploy('PlayerRegistry', owner, [owner.address]);
    const claims = await deploy('ChunkClaims', owner, [owner.address, await registry.getAddress()]);
    const marketplace = await deploy('Marketplace', owner, [owner.address, await claims.getAddress(), 500]);
    await (await claims.connect(owner).setMarketplace(await marketplace.getAddress())).wait();

    return { provider, owner, alice, bob, carol, dave, registry, claims, marketplace };
  }

  async function registerPlayers(registry, players)
  {
    for (const [handle, signer] of players)
    {
      await (await registry.connect(signer).register(handle, `ipfs://${handle}`)).wait();
    }
  }

  it('registers players, enforces unique handles, and frees handles on unregister', async function ()
  {
    const { alice, bob, registry } = await deployFixture();

    await (await registry.connect(alice).register('alice', 'ipfs://alice')).wait();

    assert.equal(await registry.isRegistered(alice.address), true);
    assert.equal(await registry.accountForHandle('alice'), alice.address);
    assert.equal(await registry.playerIdOf(alice.address), 1n);

    await expectRevert(
      () => registry.connect(bob).register('alice', 'ipfs://bob')
    );

    await (await registry.connect(alice).updateHandle('alice-prime')).wait();
    assert.equal(await registry.accountForHandle('alice'), ethers.ZeroAddress);
    assert.equal(await registry.accountForHandle('alice-prime'), alice.address);

    await (await registry.connect(alice).unregister()).wait();
    assert.equal(await registry.isRegistered(alice.address), false);

    await (await registry.connect(bob).register('alice-prime', 'ipfs://bob')).wait();
    assert.equal(await registry.accountForHandle('alice-prime'), bob.address);
    assert.equal(await registry.playerIdOf(bob.address), 2n);
  });

  it('authorizes runtime sessions and resolves runtime-facing chunk permissions', async function ()
  {
    const { provider, alice, bob, carol, registry, claims } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).claimChunk(10, 12)).wait();
    await (await claims.connect(alice).setChunkEditor(10, 12, bob.address, true)).wait();

    const latestBlock = await provider.getBlock('latest');
    const expiresAt = BigInt(latestBlock.timestamp + 300);
    await (await registry.connect(alice).authorizeRuntimeSession(carol.address, expiresAt)).wait();

    const runtimeResolution = await registry.resolveRuntimeAccount(carol.address);
    assert.equal(runtimeResolution[0], alice.address);
    assert.equal(runtimeResolution[1], false);
    assert.equal(runtimeResolution[2], true);

    const runtimeState = await claims.getChunkRuntimeState(10, 12, carol.address);
    assert.equal(runtimeState.claimed, true);
    assert.equal(runtimeState.owner, alice.address);
    assert.equal(runtimeState.ownerPlayerId, 1n);
    assert.equal(runtimeState.resolvedActor, alice.address);
    assert.equal(runtimeState.actorRegistered, true);
    assert.equal(runtimeState.actorCanEdit, true);
    assert.equal(runtimeState.actorUsesRuntimeSession, true);
    assert.equal(runtimeState.editorEpoch, 0n);

    const delegatedState = await claims.getChunkRuntimeState(10, 12, bob.address);
    assert.equal(delegatedState.resolvedActor, bob.address);
    assert.equal(delegatedState.actorCanEdit, true);
    assert.equal(delegatedState.actorUsesRuntimeSession, false);

    await provider.send('evm_increaseTime', [301]);
    await provider.send('evm_mine', []);

    const expiredState = await claims.getChunkRuntimeState(10, 12, carol.address);
    assert.equal(expiredState.resolvedActor, ethers.ZeroAddress);
    assert.equal(expiredState.actorRegistered, false);
    assert.equal(expiredState.actorCanEdit, false);
  });

  it('mints NFT-backed chunk claims and enforces registered ownership transfers', async function ()
  {
    const { alice, bob, carol, registry, claims } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await expectRevert(
      () => claims.connect(carol).claimChunk(10, 12)
    );

    await expectRevert(
      () => claims.connect(alice).claimChunk(30001, 0)
    );

    await (await claims.connect(alice).claimChunk(10, 12)).wait();
    const tokenId = await claims.tokenIdOfChunk(10, 12);

    assert.equal(tokenId, 1n);
    assert.equal(await claims.balanceOf(alice.address), 1n);
    assert.equal(await claims.ownerOf(10, 12), alice.address);
    await (await claims.connect(alice).setChunkEditor(10, 12, bob.address, true)).wait();
    assert.equal(await claims.canEdit(10, 12, bob.address), true);

    await (await claims.connect(alice).transferFrom(alice.address, bob.address, tokenId)).wait();
    assert.equal(await claims.ownerOf(10, 12), bob.address);
    assert.equal(await claims.balanceOf(alice.address), 0n);
    assert.equal(await claims.balanceOf(bob.address), 1n);
    assert.equal(await claims.canEdit(10, 12, bob.address), true);
    assert.equal(await claims.canEdit(10, 12, alice.address), false);
    assert.equal(await claims.editorEpochOfChunk(10, 12), 1n);

    await expectRevert(
      () => claims.connect(bob).transferFrom(bob.address, carol.address, tokenId)
    );
  });

  it('supports fixed-price sales for NFT chunk ownership, fee retention, and sale-state queries', async function ()
  {
    const { owner, alice, bob, registry, claims, marketplace, provider } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).claimChunk(5, -8)).wait();
    const tokenId = await claims.tokenIdOfChunk(5, -8);
    await (await marketplace.connect(alice).createListing(5, -8, ethers.parseEther('1'))).wait();

    const listing = await marketplace.listings(1);
    assert.equal(listing.tokenId, tokenId);
    assert.equal(listing.active, true);

    const saleState = await marketplace.getSaleStateForChunk(5, -8);
    assert.equal(saleState.saleKind, 1n);
    assert.equal(saleState.saleId, 1n);
    assert.equal(saleState.tokenId, tokenId);
    assert.equal(saleState.seller, alice.address);
    assert.equal(saleState.sellerStillOwnsChunk, true);
    assert.equal(saleState.price, ethers.parseEther('1'));

    await (await marketplace.connect(bob).purchaseListing(1, { value: ethers.parseEther('1') })).wait();

    assert.equal(await claims.ownerOf(5, -8), bob.address);
    assert.equal((await marketplace.listings(1)).active, false);
    assert.equal(await provider.getBalance(await marketplace.getAddress()), ethers.parseEther('0.05'));

    const clearedSaleState = await marketplace.getSaleStateForToken(tokenId);
    assert.equal(clearedSaleState.saleKind, 0n);
    assert.equal(clearedSaleState.active, false);

    await (await marketplace.connect(owner).withdrawFees(owner.address)).wait();
  });

  it('supports english auctions, refunds losing bids, settles to the winner, and exposes auction sale-state', async function ()
  {
    const { provider, alice, bob, carol, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob], ['carol', carol]]);

    await (await claims.connect(alice).claimChunk(2, 3)).wait();
    const tokenId = await claims.tokenIdOfChunk(2, 3);
    await (await marketplace.connect(alice).createAuction(2, 3, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();

    let auction = await marketplace.auctions(1);
    assert.equal(auction.tokenId, tokenId);
    assert.equal(auction.active, true);

    let saleState = await marketplace.getSaleStateForToken(tokenId);
    assert.equal(saleState.saleKind, 2n);
    assert.equal(saleState.saleId, 1n);
    assert.equal(saleState.reservePrice, ethers.parseEther('1'));
    assert.equal(saleState.minBidIncrement, ethers.parseEther('0.1'));
    assert.equal(saleState.highestBid, 0n);

    await (await marketplace.connect(bob).placeAuctionBid(1, { value: ethers.parseEther('1') })).wait();
    await (await marketplace.connect(carol).placeAuctionBid(1, { value: ethers.parseEther('1.2') })).wait();
    auction = await marketplace.auctions(1);
    assert.equal(auction.highestBidder, carol.address);
    assert.equal(auction.highestBid, ethers.parseEther('1.2'));
    assert.equal(await provider.getBalance(await marketplace.getAddress()), ethers.parseEther('1.2'));

    saleState = await marketplace.getSaleStateForChunk(2, 3);
    assert.equal(saleState.highestBidder, carol.address);
    assert.equal(saleState.highestBid, ethers.parseEther('1.2'));

    await provider.send('evm_increaseTime', [121]);
    await provider.send('evm_mine', []);

    await (await marketplace.settleAuction(1)).wait();

    auction = await marketplace.auctions(1);
    assert.equal(auction.active, false);
    assert.equal(auction.settled, true);
    assert.equal(await claims.ownerOf(2, 3), carol.address);
  });

  it('invalidates stale listings after direct token transfers and rejects unregistered buyers', async function ()
  {
    const { alice, bob, carol, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).claimChunk(9, 9)).wait();
    const tokenId = await claims.tokenIdOfChunk(9, 9);
    await (await marketplace.connect(alice).createListing(9, 9, ethers.parseEther('0.25'))).wait();

    await expectRevert(
      () => marketplace.connect(carol).purchaseListing(1, { value: ethers.parseEther('0.25') })
    );

    await (await claims.connect(alice).transferFrom(alice.address, bob.address, tokenId)).wait();

    const staleSaleState = await marketplace.getSaleStateForToken(tokenId);
    assert.equal(staleSaleState.saleKind, 1n);
    assert.equal(staleSaleState.sellerStillOwnsChunk, false);

    await (await marketplace.invalidateStaleListing(1)).wait();

    assert.equal((await marketplace.listings(1)).active, false);
    assert.equal(await claims.ownerOf(9, 9), bob.address);
  });
});
