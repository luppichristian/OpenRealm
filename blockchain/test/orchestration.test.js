const assert = require('assert');
const ganache = require('ganache');
const { ethers } = require('ethers');
const { createDeploymentRecord } = require('../scripts/deploy');
const { ORCHESTRATION_PROTOCOL_VERSION } = require('../scripts/protocolVersion');
const { compileContracts, getContractArtifact } = require('./helpers/compileContracts');

describe('OpenRealm orchestration layer', function ()
{
  this.timeout(20000);

  let compiledContracts;
  const MIN_CHUNK_PRICE = ethers.parseEther('0.01');
  const MIN_CHUNK_COORD = -30000;
  const MAX_CHUNK_COORD = 30000;
  const MAX_FEE_BPS = 2500;
  const MIN_AUCTION_DURATION = 60;

  async function deploy(contractName, signer, args = [], sourceName = `${contractName}.sol`)
  {
    const artifact = getContractArtifact(compiledContracts, sourceName, contractName);
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
    const globalParams = await deploy('GlobalParams', owner, [
      MIN_CHUNK_COORD,
      MAX_CHUNK_COORD,
      MIN_CHUNK_PRICE,
      MAX_FEE_BPS,
      MIN_AUCTION_DURATION
    ]);
    const claims = await deploy('ChunkClaims', owner, [
      owner.address,
      await registry.getAddress(),
      await globalParams.getAddress()
    ]);
    const marketplace = await deploy('Marketplace', owner, [
      owner.address,
      await claims.getAddress(),
      await globalParams.getAddress(),
      500
    ]);
    await (await claims.connect(owner).SetMarketplace(await marketplace.getAddress())).wait();

    return { provider, owner, alice, bob, carol, dave, registry, globalParams, claims, marketplace };
  }

  async function deployRevertingReceiver(fixture, deployer)
  {
    return deploy(
      'RevertingReceiver',
      deployer,
      [
        await fixture.registry.getAddress(),
        await fixture.claims.getAddress(),
        await fixture.marketplace.getAddress()
      ],
      'test/RevertingReceiver.sol'
    );
  }

  async function registerPlayers(registry, players)
  {
    for (const [handle, signer] of players)
    {
      await (await registry.connect(signer).Register(handle, `ipfs://${handle}`)).wait();
    }
  }

  async function getLatestBalance(provider, account)
  {
    return BigInt(await provider.send('eth_getBalance', [account, 'latest']));
  }

  it('defines an explicit orchestration protocol version for deployment records', async function ()
  {
    const deploymentRecord = createDeploymentRecord({
      networkName: 'local',
      chainId: 31337,
      ownerAddress: '0x0000000000000000000000000000000000000001',
      deployerAddress: '0x0000000000000000000000000000000000000002',
      minChunkCoord: MIN_CHUNK_COORD,
      maxChunkCoord: MAX_CHUNK_COORD,
      minChunkPrice: MIN_CHUNK_PRICE,
      maxFeeBps: MAX_FEE_BPS,
      minAuctionDuration: MIN_AUCTION_DURATION,
      feeBps: 500,
      registryAddress: '0x0000000000000000000000000000000000000011',
      globalParamsAddress: '0x0000000000000000000000000000000000000012',
      claimsAddress: '0x0000000000000000000000000000000000000013',
      marketplaceAddress: '0x0000000000000000000000000000000000000014'
    });

    assert.equal(ORCHESTRATION_PROTOCOL_VERSION, 1);
    assert.equal(deploymentRecord.protocolVersion, ORCHESTRATION_PROTOCOL_VERSION);
  });

  it('registers players, treats handles case-insensitively, and frees handles after unregistering', async function ()
  {
    const { alice, bob, registry } = await deployFixture();

    await (await registry.connect(alice).Register('Alice', 'ipfs://alice')).wait();

    assert.equal(await registry.IsRegistered(alice.address), true);
    assert.equal(await registry.AccountForHandle('alice'), alice.address);
    assert.equal(await registry.AccountForHandle('ALICE'), alice.address);
    assert.equal(await registry.PlayerIdOf(alice.address), 1n);
    assert.equal(await registry.NormalizedHandleHash('Alice'), await registry.NormalizedHandleHash('aLiCe'));

    await expectRevert(
      () => registry.connect(bob).Register('alice', 'ipfs://bob')
    );

    await (await registry.connect(alice).UpdateHandle('alice-prime')).wait();
    assert.equal(await registry.AccountForHandle('alice'), ethers.ZeroAddress);
    assert.equal(await registry.AccountForHandle('ALICE-PRIME'), alice.address);

    await (await registry.connect(alice).Unregister()).wait();
    assert.equal(await registry.IsRegistered(alice.address), false);

    await (await registry.connect(bob).Register('ALICE-PRIME', 'ipfs://bob')).wait();
    assert.equal(await registry.AccountForHandle('alice-prime'), bob.address);
    assert.equal(await registry.PlayerIdOf(bob.address), 2n);
  });

  it('authorizes runtime sessions and resolves runtime-facing chunk permissions', async function ()
  {
    const { provider, alice, bob, carol, registry, claims } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(10, 12, { value: MIN_CHUNK_PRICE })).wait();
    await (await claims.connect(alice).SetChunkEditor(10, 12, bob.address, true)).wait();

    const latestBlock = await provider.getBlock('latest');
    const expiresAt = BigInt(latestBlock.timestamp + 300);
    await (await registry.connect(alice).AuthorizeRuntimeSession(carol.address, expiresAt)).wait();

    const runtimeResolution = await registry.ResolveRuntimeAccount(carol.address);
    assert.equal(runtimeResolution[0], alice.address);
    assert.equal(runtimeResolution[1], false);
    assert.equal(runtimeResolution[2], true);

    const runtimeState = await claims.GetChunkRuntimeState(10, 12, carol.address);
    assert.equal(runtimeState.claimed, true);
    assert.equal(runtimeState.owner, alice.address);
    assert.equal(runtimeState.ownerPlayerId, 1n);
    assert.equal(runtimeState.resolvedActor, alice.address);
    assert.equal(runtimeState.actorRegistered, true);
    assert.equal(runtimeState.actorCanEdit, true);
    assert.equal(runtimeState.actorUsesRuntimeSession, true);
    assert.equal(runtimeState.editorEpoch, 0n);

    const delegatedState = await claims.GetChunkRuntimeState(10, 12, bob.address);
    assert.equal(delegatedState.resolvedActor, bob.address);
    assert.equal(delegatedState.actorCanEdit, true);
    assert.equal(delegatedState.actorUsesRuntimeSession, false);

    await provider.send('evm_increaseTime', [301]);
    await provider.send('evm_mine', []);

    const expiredState = await claims.GetChunkRuntimeState(10, 12, carol.address);
    assert.equal(expiredState.resolvedActor, ethers.ZeroAddress);
    assert.equal(expiredState.actorRegistered, false);
    assert.equal(expiredState.actorCanEdit, false);
  });

  it('mints NFT-backed paid chunk claims, enforces coordinate/payment rules, and keeps registered-only transfers', async function ()
  {
    const { alice, bob, carol, registry, claims } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await expectRevert(
      () => claims.connect(carol).ClaimChunk(10, 12, { value: MIN_CHUNK_PRICE })
    );

    await expectRevert(
      () => claims.connect(alice).ClaimChunk(30001, 0, { value: MIN_CHUNK_PRICE })
    );

    await expectRevert(
      () => claims.connect(alice).ClaimChunk(10, 12)
    );

    await (await claims.connect(alice).ClaimChunk(10, 12, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(10, 12);

    assert.equal(tokenId, 1n);
    assert.equal(await claims.balanceOf(alice.address), 1n);
    assert.equal(await claims.OwnerOf(10, 12), alice.address);
    await (await claims.connect(alice).SetChunkEditor(10, 12, bob.address, true)).wait();
    assert.equal(await claims.CanEdit(10, 12, bob.address), true);

    await (await claims.connect(alice).transferFrom(alice.address, bob.address, tokenId)).wait();
    assert.equal(await claims.OwnerOf(10, 12), bob.address);
    assert.equal(await claims.balanceOf(alice.address), 0n);
    assert.equal(await claims.balanceOf(bob.address), 1n);
    assert.equal(await claims.CanEdit(10, 12, bob.address), true);
    assert.equal(await claims.CanEdit(10, 12, alice.address), false);
    assert.equal(await claims.EditorEpochOfChunk(10, 12), 1n);

    await expectRevert(
      () => claims.connect(bob).transferFrom(bob.address, carol.address, tokenId)
    );
  });

  it('supports fixed-price sales with pull-based seller proceeds and fee withdrawal accounting', async function ()
  {
    const { owner, alice, bob, registry, claims, marketplace, provider } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(5, -8, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(5, -8);

    await expectRevert(
      () => marketplace.connect(alice).CreateListing(5, -8, MIN_CHUNK_PRICE - 1n)
    );

    await (await marketplace.connect(alice).CreateListing(5, -8, ethers.parseEther('1'))).wait();

    const listing = await marketplace.listings(1);
    assert.equal(listing.tokenId, tokenId);
    assert.equal(listing.active, true);

    const saleState = await marketplace.GetSaleStateForChunk(5, -8);
    assert.equal(saleState.saleKind, 1n);
    assert.equal(saleState.saleId, 1n);
    assert.equal(saleState.tokenId, tokenId);
    assert.equal(saleState.seller, alice.address);
    assert.equal(saleState.sellerStillOwnsChunk, true);
    assert.equal(saleState.price, ethers.parseEther('1'));

    await (await marketplace.connect(bob).PurchaseListing(1, { value: ethers.parseEther('1') })).wait();

    assert.equal(await claims.OwnerOf(5, -8), bob.address);
    assert.equal((await marketplace.listings(1)).active, false);
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), ethers.parseEther('1'));
    assert.equal(await marketplace.accruedProtocolFees(), ethers.parseEther('0.05'));
    assert.equal(await marketplace.withdrawableBalances(alice.address), ethers.parseEther('0.95'));

    const clearedSaleState = await marketplace.GetSaleStateForToken(tokenId);
    assert.equal(clearedSaleState.saleKind, 0n);
    assert.equal(clearedSaleState.active, false);

    await expectRevert(
      () => marketplace.connect(alice).WithdrawBalance(ethers.ZeroAddress)
    );
    await expectRevert(
      () => marketplace.connect(owner).WithdrawFees(ethers.ZeroAddress)
    );

    await (await marketplace.connect(alice).WithdrawBalance(alice.address)).wait();
    assert.equal(await marketplace.withdrawableBalances(alice.address), 0n);
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), ethers.parseEther('0.05'));

    await (await marketplace.connect(owner).WithdrawFees(owner.address)).wait();
    assert.equal(await marketplace.accruedProtocolFees(), 0n);
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), 0n);
  });

  it('supports English auctions with credited refunds, pull-based seller proceeds, and sale-state reads', async function ()
  {
    const { provider, owner, alice, bob, carol, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob], ['carol', carol]]);

    await (await claims.connect(alice).ClaimChunk(2, 3, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(2, 3);

    await expectRevert(
      () => marketplace.connect(alice).CreateAuction(2, 3, MIN_CHUNK_PRICE - 1n, ethers.parseEther('0.1'), 120)
    );

    await (await marketplace.connect(alice).CreateAuction(2, 3, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();

    let auction = await marketplace.auctions(1);
    assert.equal(auction.tokenId, tokenId);
    assert.equal(auction.active, true);

    let saleState = await marketplace.GetSaleStateForToken(tokenId);
    assert.equal(saleState.saleKind, 2n);
    assert.equal(saleState.saleId, 1n);
    assert.equal(saleState.reservePrice, ethers.parseEther('1'));
    assert.equal(saleState.minBidIncrement, ethers.parseEther('0.1'));
    assert.equal(saleState.highestBid, 0n);

    await (await marketplace.connect(bob).PlaceAuctionBid(1, { value: ethers.parseEther('1') })).wait();
    await (await marketplace.connect(carol).PlaceAuctionBid(1, { value: ethers.parseEther('1.2') })).wait();
    auction = await marketplace.auctions(1);
    assert.equal(auction.highestBidder, carol.address);
    assert.equal(auction.highestBid, ethers.parseEther('1.2'));
    assert.equal(await marketplace.withdrawableBalances(bob.address), ethers.parseEther('1'));
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), ethers.parseEther('2.2'));

    saleState = await marketplace.GetSaleStateForChunk(2, 3);
    assert.equal(saleState.highestBidder, carol.address);
    assert.equal(saleState.highestBid, ethers.parseEther('1.2'));

    await provider.send('evm_increaseTime', [121]);
    await provider.send('evm_mine', []);

    await (await marketplace.SettleAuction(1)).wait();

    auction = await marketplace.auctions(1);
    assert.equal(auction.active, false);
    assert.equal(auction.settled, true);
    assert.equal(await claims.OwnerOf(2, 3), carol.address);
    assert.equal(await marketplace.accruedProtocolFees(), ethers.parseEther('0.06'));
    assert.equal(await marketplace.withdrawableBalances(alice.address), ethers.parseEther('1.14'));

    await (await marketplace.connect(bob).WithdrawBalance(bob.address)).wait();
    await (await marketplace.connect(alice).WithdrawBalance(alice.address)).wait();
    await (await marketplace.connect(owner).WithdrawFees(owner.address)).wait();
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), 0n);
  });

  it('lets buyers purchase listings even when the seller contract refuses ETH and preserves seller withdrawals', async function ()
  {
    const fixture = await deployFixture();
    const { provider, owner, bob, registry, claims, marketplace } = fixture;
    await registerPlayers(registry, [['bob', bob]]);

    const seller = await deployRevertingReceiver(fixture, owner);
    await (await seller.connect(owner).Register('seller-contract', 'ipfs://seller-contract')).wait();
    await (await seller.connect(owner).ClaimChunk(7, 7, { value: MIN_CHUNK_PRICE })).wait();
    await (await seller.connect(owner).CreateListing(7, 7, ethers.parseEther('0.5'))).wait();

    await (await marketplace.connect(bob).PurchaseListing(1, { value: ethers.parseEther('0.5') })).wait();

    assert.equal(await claims.OwnerOf(7, 7), bob.address);
    assert.equal(await marketplace.withdrawableBalances(await seller.getAddress()), ethers.parseEther('0.475'));
    assert.equal(await marketplace.accruedProtocolFees(), ethers.parseEther('0.025'));

    const ownerBalanceBefore = await getLatestBalance(provider, owner.address);
    const withdrawTx = await seller.connect(owner).WithdrawMarketplaceBalance(owner.address);
    const withdrawReceipt = await withdrawTx.wait();
    const ownerBalanceAfter = await getLatestBalance(provider, owner.address);
    assert(ownerBalanceAfter > ownerBalanceBefore - withdrawReceipt.gasUsed * withdrawReceipt.gasPrice);
  });

  it('lets higher bids replace a reverting bidder and preserves a pull-based refund', async function ()
  {
    const fixture = await deployFixture();
    const { provider, owner, alice, carol, registry, claims, marketplace } = fixture;
    await registerPlayers(registry, [['alice', alice], ['carol', carol]]);

    const bidder = await deployRevertingReceiver(fixture, owner);
    await (await bidder.connect(owner).Register('refund-bidder', 'ipfs://refund-bidder')).wait();

    await (await claims.connect(alice).ClaimChunk(11, 5, { value: MIN_CHUNK_PRICE })).wait();
    await (await marketplace.connect(alice).CreateAuction(11, 5, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();

    await (await bidder.connect(owner).PlaceAuctionBid(1, { value: ethers.parseEther('1') })).wait();
    await (await marketplace.connect(carol).PlaceAuctionBid(1, { value: ethers.parseEther('1.2') })).wait();

    assert.equal(await marketplace.withdrawableBalances(await bidder.getAddress()), ethers.parseEther('1'));

    const ownerBalanceBefore = await getLatestBalance(provider, owner.address);
    const withdrawTx = await bidder.connect(owner).WithdrawMarketplaceBalance(owner.address);
    const withdrawReceipt = await withdrawTx.wait();
    const ownerBalanceAfter = await getLatestBalance(provider, owner.address);
    assert(ownerBalanceAfter > ownerBalanceBefore - withdrawReceipt.gasUsed * withdrawReceipt.gasPrice);
  });

  it('settles auctions even when the seller contract refuses ETH and preserves pull-based proceeds', async function ()
  {
    const fixture = await deployFixture();
    const { provider, owner, bob, registry, claims, marketplace } = fixture;
    await registerPlayers(registry, [['bob', bob]]);

    const seller = await deployRevertingReceiver(fixture, owner);
    await (await seller.connect(owner).Register('auction-seller', 'ipfs://auction-seller')).wait();
    await (await seller.connect(owner).ClaimChunk(13, -2, { value: MIN_CHUNK_PRICE })).wait();
    await (await seller.connect(owner).CreateAuction(13, -2, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();

    await (await marketplace.connect(bob).PlaceAuctionBid(1, { value: ethers.parseEther('1') })).wait();
    await provider.send('evm_increaseTime', [121]);
    await provider.send('evm_mine', []);

    await (await marketplace.SettleAuction(1)).wait();

    assert.equal(await claims.OwnerOf(13, -2), bob.address);
    assert.equal(await marketplace.withdrawableBalances(await seller.getAddress()), ethers.parseEther('0.95'));
    assert.equal(await marketplace.accruedProtocolFees(), ethers.parseEther('0.05'));

    const ownerBalanceBefore = await getLatestBalance(provider, owner.address);
    const withdrawTx = await seller.connect(owner).WithdrawMarketplaceBalance(owner.address);
    const withdrawReceipt = await withdrawTx.wait();
    const ownerBalanceAfter = await getLatestBalance(provider, owner.address);
    assert(ownerBalanceAfter > ownerBalanceBefore - withdrawReceipt.gasUsed * withdrawReceipt.gasPrice);
  });

  it('invalidates stale auctions even when the prior bidder contract refuses ETH', async function ()
  {
    const fixture = await deployFixture();
    const { owner, alice, bob, registry, claims, marketplace } = fixture;
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    const bidder = await deployRevertingReceiver(fixture, owner);
    await (await bidder.connect(owner).Register('stale-bidder', 'ipfs://stale-bidder')).wait();

    await (await claims.connect(alice).ClaimChunk(15, 1, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(15, 1);
    await (await marketplace.connect(alice).CreateAuction(15, 1, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();
    await (await bidder.connect(owner).PlaceAuctionBid(1, { value: ethers.parseEther('1') })).wait();

    await (await claims.connect(alice).transferFrom(alice.address, bob.address, tokenId)).wait();
    await (await marketplace.InvalidateStaleAuction(1)).wait();

    assert.equal((await marketplace.auctions(1)).active, false);
    assert.equal(await marketplace.withdrawableBalances(await bidder.getAddress()), ethers.parseEther('1'));
  });

  it('invalidates stale listings after direct token transfers and rejects unregistered buyers', async function ()
  {
    const { alice, bob, carol, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(9, 9, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(9, 9);
    await (await marketplace.connect(alice).CreateListing(9, 9, ethers.parseEther('0.25'))).wait();

    await expectRevert(
      () => marketplace.connect(carol).PurchaseListing(1, { value: ethers.parseEther('0.25') })
    );

    await (await claims.connect(alice).transferFrom(alice.address, bob.address, tokenId)).wait();

    const staleSaleState = await marketplace.GetSaleStateForToken(tokenId);
    assert.equal(staleSaleState.saleKind, 1n);
    assert.equal(staleSaleState.sellerStillOwnsChunk, false);

    await (await marketplace.InvalidateStaleListing(1)).wait();

    assert.equal((await marketplace.listings(1)).active, false);
    assert.equal(await claims.OwnerOf(9, 9), bob.address);
  });

  it('refunds the minimum chunk purchase price when abandoning and returns the chunk to the free pool', async function ()
  {
    const { provider, alice, bob, registry, claims } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(4, 4, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(4, 4);
    const contractBalanceBeforeAbandon = await getLatestBalance(provider, await claims.getAddress());

    const abandonTx = await claims.connect(alice).AbandonChunk(4, 4);
    await abandonTx.wait();
    const contractBalanceAfterAbandon = await getLatestBalance(provider, await claims.getAddress());

    assert.equal(contractBalanceBeforeAbandon, MIN_CHUNK_PRICE);
    assert.equal(contractBalanceAfterAbandon, 0n);
    assert.equal(await claims.TokenIdOfChunk(4, 4), 0n);
    assert.equal(await claims.OwnerOf(4, 4), ethers.ZeroAddress);

    await expectRevert(
      () => claims.connect(bob).ClaimChunk(4, 4)
    );

    await (await claims.connect(bob).ClaimChunk(4, 4, { value: MIN_CHUNK_PRICE })).wait();
    assert.equal(await claims.OwnerOf(4, 4), bob.address);
    assert.equal(await claims.TokenIdOfChunk(4, 4), tokenId + 1n);
  });
});
