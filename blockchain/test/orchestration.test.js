const assert = require('assert');
const ganache = require('ganache');
const { ethers } = require('ethers');
const { createDeploymentRecord } = require('../scripts/deploy');
const { ORCHESTRATION_PROTOCOL_VERSION } = require('../scripts/protocolVersion');
const { compileContracts, getContractArtifact } = require('./helpers/compileContracts');

const GENERIC_ERROR_INTERFACE = new ethers.Interface([
  'error Error(string)',
  'error Panic(uint256)'
]);

describe('OpenRealm orchestration layer', function ()
{
  this.timeout(20000);

  let compiledContracts;
  const MIN_CHUNK_PRICE = ethers.parseEther('0.01');
  const MIN_CHUNK_COORD = -30000;
  const MAX_CHUNK_COORD = 30000;
  const MAX_FEE_BPS = 2500;
  const MIN_AUCTION_DURATION = 60;

  function makeFactory(contractName, signer, sourceName = `${contractName}.sol`)
  {
    const artifact = getContractArtifact(compiledContracts, sourceName, contractName);
    return new ethers.ContractFactory(artifact.abi, artifact.bytecode, signer);
  }

  async function deploy(contractName, signer, args = [], sourceName = `${contractName}.sol`)
  {
    const factory = makeFactory(contractName, signer, sourceName);
    const contract = await factory.deploy(...args);
    await contract.waitForDeployment();
    return contract;
  }

  function getErrorData(error)
  {
    return error.data || error.info?.error?.data?.result || error.info?.error?.data || null;
  }

  function normalizeErrorValue(value)
  {
    if (typeof value === 'bigint' || typeof value === 'number')
    {
      return value.toString();
    }
    if (typeof value === 'string' && /^0x[0-9a-fA-F]{40}$/.test(value))
    {
      return value.toLowerCase();
    }
    return String(value);
  }

  function parseRevert(error, contractInterface)
  {
    const data = getErrorData(error);
    const candidateInterfaces = [contractInterface, GENERIC_ERROR_INTERFACE].filter(Boolean);

    if (data && data !== '0x')
    {
      for (const iface of candidateInterfaces)
      {
        try
        {
          return iface.parseError(data);
        }
        catch (_error)
        {
        }
      }
    }

    return null;
  }

  async function expectCustomError(action, contractInterface, expectedName, expectedArgs = null)
  {
    try
    {
      await action();
      assert.fail(`Expected revert with ${expectedName}`);
    }
    catch (error)
    {
      const parsed = parseRevert(error, contractInterface);
      assert.ok(parsed, `Expected revert data for ${expectedName}, got: ${error.shortMessage || error.message}`);
      assert.equal(parsed.name, expectedName);

      if (!expectedArgs)
      {
        return;
      }

      const actualArgs = Array.from(parsed.args);
      assert.equal(actualArgs.length, expectedArgs.length);
      for (let index = 0; index < expectedArgs.length; index += 1)
      {
        const expected = expectedArgs[index];
        const actual = actualArgs[index];
        if (expected instanceof RegExp)
        {
          assert.match(normalizeErrorValue(actual), expected);
          continue;
        }

        assert.equal(normalizeErrorValue(actual), normalizeErrorValue(expected));
      }
    }
  }

  async function expectDeploymentCustomError(
    contractName,
    signer,
    args,
    expectedName,
    expectedArgs = null,
    sourceName = `${contractName}.sol`
  )
  {
    const factory = makeFactory(contractName, signer, sourceName);
    const deployTransaction = await factory.getDeployTransaction(...args);
    await expectCustomError(
      () => signer.provider.call(deployTransaction),
      factory.interface,
      expectedName,
      expectedArgs
    );
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

  async function expectNetBalanceIncrease(provider, account, txPromise, expectedIncrease)
  {
    const beforeBalance = await getLatestBalance(provider, account);
    const tx = await txPromise;
    const receipt = await tx.wait();
    const gasPrice = receipt.gasPrice ?? receipt.effectiveGasPrice;
    const gasCost = receipt.gasUsed * gasPrice;
    const afterBalance = await getLatestBalance(provider, account);
    assert.equal(afterBalance - beforeBalance + gasCost, expectedIncrease);
  }

  it('defines an explicit orchestration protocol version and complete deployment record shape', async function ()
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
    assert.equal(deploymentRecord.network, 'local');
    assert.equal(deploymentRecord.chainId, 31337);
    assert.equal(deploymentRecord.ownerAddress, '0x0000000000000000000000000000000000000001');
    assert.equal(deploymentRecord.deployerAddress, '0x0000000000000000000000000000000000000002');
    assert.equal(deploymentRecord.globalParams.minChunkCoord, MIN_CHUNK_COORD);
    assert.equal(deploymentRecord.globalParams.maxChunkCoord, MAX_CHUNK_COORD);
    assert.equal(deploymentRecord.globalParams.minChunkPriceWei, MIN_CHUNK_PRICE.toString());
    assert.equal(deploymentRecord.globalParams.maxFeeBps, MAX_FEE_BPS);
    assert.equal(deploymentRecord.globalParams.minAuctionDurationSeconds, MIN_AUCTION_DURATION);
    assert.equal(deploymentRecord.feeBps, 500);
    assert.deepEqual(deploymentRecord.contracts, {
      PlayerRegistry: '0x0000000000000000000000000000000000000011',
      GlobalParams: '0x0000000000000000000000000000000000000012',
      ChunkClaims: '0x0000000000000000000000000000000000000013',
      Marketplace: '0x0000000000000000000000000000000000000014'
    });
    assert.ok(!Number.isNaN(Date.parse(deploymentRecord.deployedAt)));
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

    await expectCustomError(
      () => registry.connect(bob).Register.staticCall('alice', 'ipfs://bob'),
      registry.interface,
      'HandleAlreadyTaken',
      ['alice']
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

  it('rejects invalid registry operations and supports runtime-session revocation and metadata updates', async function ()
  {
    const { provider, alice, bob, carol, registry } = await deployFixture();
    const latestBlock = await provider.getBlock('latest');

    await expectCustomError(
      () => registry.connect(alice).Register.staticCall('', 'ipfs://alice'),
      registry.interface,
      'EmptyHandle'
    );

    await (await registry.connect(alice).Register('alice', 'ipfs://alice')).wait();
    await expectCustomError(
      () => registry.connect(alice).Register.staticCall('alice-2', 'ipfs://alice-2'),
      registry.interface,
      'AlreadyRegistered',
      [alice.address]
    );

    await (await registry.connect(alice).UpdateMetadataURI('ipfs://alice-v2')).wait();
    const profile = await registry.GetProfile(alice.address);
    assert.equal(profile[2], 'ipfs://alice-v2');
    assert.equal(profile[3], true);

    await (await registry.connect(bob).Register('bob', 'ipfs://bob')).wait();

    await expectCustomError(
      () => registry.connect(alice).AuthorizeRuntimeSession.staticCall(ethers.ZeroAddress, BigInt(latestBlock.timestamp + 60)),
      registry.interface,
      'InvalidRuntimeSession',
      [ethers.ZeroAddress]
    );
    await expectCustomError(
      () => registry.connect(alice).AuthorizeRuntimeSession.staticCall(alice.address, BigInt(latestBlock.timestamp + 60)),
      registry.interface,
      'InvalidRuntimeSession',
      [alice.address]
    );
    await expectCustomError(
      () => registry.connect(alice).AuthorizeRuntimeSession.staticCall(carol.address, BigInt(latestBlock.timestamp)),
      registry.interface,
      'InvalidRuntimeSessionExpiry',
      [BigInt(latestBlock.timestamp)]
    );

    await (await registry.connect(alice).AuthorizeRuntimeSession(carol.address, BigInt(latestBlock.timestamp + 120))).wait();

    await expectCustomError(
      () => registry.connect(bob).AuthorizeRuntimeSession.staticCall(carol.address, BigInt(latestBlock.timestamp + 240)),
      registry.interface,
      'RuntimeSessionAlreadyBound',
      [carol.address, alice.address]
    );
    await expectCustomError(
      () => registry.connect(bob).RevokeRuntimeSession.staticCall(carol.address),
      registry.interface,
      'RuntimeSessionNotOwnedByAccount',
      [bob.address, carol.address]
    );

    await (await registry.connect(alice).RevokeRuntimeSession(carol.address)).wait();
    const revokedSession = await registry.GetRuntimeSession(carol.address);
    assert.equal(revokedSession[0], ethers.ZeroAddress);
    assert.equal(revokedSession[2], false);
    const revokedResolution = await registry.ResolveRuntimeAccount(carol.address);
    assert.equal(revokedResolution[0], ethers.ZeroAddress);
    assert.equal(revokedResolution[2], false);
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

  it('enforces chunk claim, editor, marketplace, and claim-view invariants', async function ()
  {
    const { alice, bob, carol, registry, claims } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await expectCustomError(
      () => claims.connect(carol).ClaimChunk.staticCall(10, 12, { value: MIN_CHUNK_PRICE }),
      claims.interface,
      'NotRegisteredPlayer',
      [carol.address]
    );
    await expectCustomError(
      () => claims.connect(alice).ClaimChunk.staticCall(30001, 0, { value: MIN_CHUNK_PRICE }),
      claims.interface,
      'InvalidChunkCoordinate',
      [30001, 0]
    );
    await expectCustomError(
      () => claims.connect(alice).ClaimChunk.staticCall(10, 12),
      claims.interface,
      'IncorrectChunkPurchasePrice',
      [MIN_CHUNK_PRICE, 0n]
    );

    await (await claims.connect(alice).ClaimChunk(10, 12, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(10, 12);

    await expectCustomError(
      () => claims.connect(bob).ClaimChunk.staticCall(10, 12, { value: MIN_CHUNK_PRICE }),
      claims.interface,
      'ChunkAlreadyClaimed',
      [10, 12]
    );

    const claim = await claims.GetClaim(10, 12);
    assert.equal(claim.tokenId, tokenId);
    assert.equal(claim.x, 10);
    assert.equal(claim.y, 12);
    assert.ok(claim.claimedAt > 0n);
    assert.equal((await claims.GetClaimByTokenId(tokenId)).tokenId, tokenId);
    assert.equal(await claims.tokenURI(tokenId), 'openrealm://chunk-claim');

    await expectCustomError(
      () => claims.GetClaimByTokenId.staticCall(999n),
      claims.interface,
      'UnknownChunkToken',
      [999n]
    );
    await expectCustomError(
      () => claims.tokenURI.staticCall(999n),
      claims.interface,
      'ERC721NonexistentToken',
      [999n]
    );
    await expectCustomError(
      () => claims.connect(alice).SetChunkEditor.staticCall(10, 12, ethers.ZeroAddress, true),
      claims.interface,
      'CannotDelegateToZeroAddress'
    );
    await expectCustomError(
      () => claims.connect(alice).MarketplaceTransferChunk.staticCall(10, 12, alice.address, bob.address),
      claims.interface,
      'UnauthorizedMarketplace',
      [alice.address]
    );

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

    await expectCustomError(
      () => claims.connect(bob).transferFrom.staticCall(bob.address, carol.address, tokenId),
      claims.interface,
      'NotRegisteredPlayer',
      [carol.address]
    );

    await (await claims.connect(alice).ClaimChunk(MIN_CHUNK_COORD, MIN_CHUNK_COORD, { value: MIN_CHUNK_PRICE })).wait();
    await (await claims.connect(bob).ClaimChunk(MAX_CHUNK_COORD, MAX_CHUNK_COORD, { value: MIN_CHUNK_PRICE })).wait();
    assert.equal(await claims.OwnerOf(MIN_CHUNK_COORD, MIN_CHUNK_COORD), alice.address);
    assert.equal(await claims.OwnerOf(MAX_CHUNK_COORD, MAX_CHUNK_COORD), bob.address);
  });

  it('validates constructor guards and owner-only settings for global params, claims, and marketplace', async function ()
  {
    const { owner, alice, registry, globalParams, claims, marketplace } = await deployFixture();

    await expectDeploymentCustomError('GlobalParams', owner, [10, 0, 1n, 2500, 60], 'InvalidChunkCoordRange', [10, 0]);
    await expectDeploymentCustomError('GlobalParams', owner, [0, 10, 0n, 2500, 60], 'InvalidChunkPrice', [0n]);
    await expectDeploymentCustomError('GlobalParams', owner, [0, 10, 1n, 10001, 60], 'InvalidFeeBpsLimit', [10001]);
    await expectDeploymentCustomError('GlobalParams', owner, [0, 10, 1n, 2500, 0], 'InvalidAuctionDurationLimit', [0]);

    await expectDeploymentCustomError(
      'ChunkClaims',
      owner,
      [owner.address, ethers.ZeroAddress, await globalParams.getAddress()],
      'NotRegisteredPlayer',
      [ethers.ZeroAddress]
    );
    await expectDeploymentCustomError(
      'ChunkClaims',
      owner,
      [owner.address, await registry.getAddress(), ethers.ZeroAddress],
      'InvalidGlobalParams',
      [ethers.ZeroAddress]
    );

    await expectDeploymentCustomError(
      'Marketplace',
      owner,
      [owner.address, ethers.ZeroAddress, await globalParams.getAddress(), 500],
      'InvalidChunkClaims',
      [ethers.ZeroAddress]
    );
    await expectDeploymentCustomError(
      'Marketplace',
      owner,
      [owner.address, await claims.getAddress(), ethers.ZeroAddress, 500],
      'InvalidGlobalParams',
      [ethers.ZeroAddress]
    );
    await expectDeploymentCustomError(
      'Marketplace',
      owner,
      [owner.address, await claims.getAddress(), await globalParams.getAddress(), MAX_FEE_BPS + 1],
      'InvalidFeeBps',
      [MAX_FEE_BPS + 1]
    );

    const marketplaceAddress = await marketplace.getAddress();
    await expectCustomError(
      () => claims.connect(alice).SetMarketplace.staticCall(marketplaceAddress),
      claims.interface,
      'OwnableUnauthorizedAccount',
      [alice.address]
    );
    await expectCustomError(
      () => claims.connect(owner).SetMarketplace.staticCall(ethers.ZeroAddress),
      claims.interface,
      'InvalidMarketplace',
      [ethers.ZeroAddress]
    );
    await expectCustomError(
      () => marketplace.connect(alice).SetFeeBps.staticCall(100),
      marketplace.interface,
      'OwnableUnauthorizedAccount',
      [alice.address]
    );
    await expectCustomError(
      () => marketplace.connect(owner).SetFeeBps.staticCall(MAX_FEE_BPS + 1),
      marketplace.interface,
      'InvalidFeeBps',
      [MAX_FEE_BPS + 1]
    );

    await (await marketplace.connect(owner).SetFeeBps(750)).wait();
    assert.equal(await marketplace.feeBps(), 750n);
  });

  it('supports fixed-price sales with pull-based seller proceeds and fee withdrawal accounting', async function ()
  {
    const { owner, alice, bob, registry, claims, marketplace, provider } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(5, -8, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(5, -8);

    await expectCustomError(
      () => marketplace.connect(alice).CreateListing.staticCall(5, -8, MIN_CHUNK_PRICE - 1n),
      marketplace.interface,
      'InvalidPrice',
      [MIN_CHUNK_PRICE - 1n]
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

    await expectCustomError(
      () => marketplace.connect(alice).WithdrawBalance.staticCall(ethers.ZeroAddress),
      marketplace.interface,
      'InvalidRecipient',
      [ethers.ZeroAddress]
    );
    await expectCustomError(
      () => marketplace.connect(owner).WithdrawFees.staticCall(ethers.ZeroAddress),
      marketplace.interface,
      'InvalidRecipient',
      [ethers.ZeroAddress]
    );

    await expectNetBalanceIncrease(
      provider,
      alice.address,
      marketplace.connect(alice).WithdrawBalance(alice.address),
      ethers.parseEther('0.95')
    );
    assert.equal(await marketplace.withdrawableBalances(alice.address), 0n);
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), ethers.parseEther('0.05'));

    await expectNetBalanceIncrease(
      provider,
      owner.address,
      marketplace.connect(owner).WithdrawFees(owner.address),
      ethers.parseEther('0.05')
    );
    assert.equal(await marketplace.accruedProtocolFees(), 0n);
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), 0n);
  });

  it('enforces listing lifecycle rules, duplicate-sale prevention, and seller-only cancellation', async function ()
  {
    const { alice, bob, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(6, 6, { value: MIN_CHUNK_PRICE })).wait();

    await (await marketplace.connect(alice).CreateListing(6, 6, ethers.parseEther('0.5'))).wait();
    await expectCustomError(
      () => marketplace.connect(alice).CreateAuction.staticCall(6, 6, ethers.parseEther('1'), ethers.parseEther('0.1'), 120),
      marketplace.interface,
      'SaleAlreadyExists',
      [1n]
    );
    await expectCustomError(
      () => marketplace.connect(bob).PurchaseListing.staticCall(1, { value: ethers.parseEther('0.49') }),
      marketplace.interface,
      'IncorrectPayment',
      [ethers.parseEther('0.5'), ethers.parseEther('0.49')]
    );
    await expectCustomError(
      () => marketplace.connect(bob).CancelListing.staticCall(1),
      marketplace.interface,
      'NotListingSeller',
      [bob.address, 1n]
    );
    await expectCustomError(
      () => marketplace.InvalidateStaleListing.staticCall(1),
      marketplace.interface,
      'SellerNoLongerOwnsChunk',
      [0n]
    );

    await (await marketplace.connect(alice).CancelListing(1)).wait();
    assert.equal((await marketplace.listings(1)).active, false);
  });

  it('supports English auctions with credited refunds, pull-based seller proceeds, and sale-state reads', async function ()
  {
    const { provider, owner, alice, bob, carol, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob], ['carol', carol]]);

    await (await claims.connect(alice).ClaimChunk(2, 3, { value: MIN_CHUNK_PRICE })).wait();
    const tokenId = await claims.TokenIdOfChunk(2, 3);

    await expectCustomError(
      () => marketplace.connect(alice).CreateAuction.staticCall(2, 3, MIN_CHUNK_PRICE - 1n, ethers.parseEther('0.1'), 120),
      marketplace.interface,
      'InvalidPrice',
      [MIN_CHUNK_PRICE - 1n]
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

    await expectNetBalanceIncrease(
      provider,
      bob.address,
      marketplace.connect(bob).WithdrawBalance(bob.address),
      ethers.parseEther('1')
    );
    await expectNetBalanceIncrease(
      provider,
      alice.address,
      marketplace.connect(alice).WithdrawBalance(alice.address),
      ethers.parseEther('1.14')
    );
    await expectNetBalanceIncrease(
      provider,
      owner.address,
      marketplace.connect(owner).WithdrawFees(owner.address),
      ethers.parseEther('0.06')
    );
    assert.equal(await getLatestBalance(provider, await marketplace.getAddress()), 0n);
  });

  it('enforces auction lifecycle rules, seller-only cancellation, and no-bid settlement behavior', async function ()
  {
    const { provider, alice, bob, registry, claims, marketplace } = await deployFixture();
    await registerPlayers(registry, [['alice', alice], ['bob', bob]]);

    await (await claims.connect(alice).ClaimChunk(3, 4, { value: MIN_CHUNK_PRICE })).wait();
    await expectCustomError(
      () => marketplace.connect(alice).CreateAuction.staticCall(3, 4, ethers.parseEther('1'), ethers.parseEther('0.1'), MIN_AUCTION_DURATION - 1),
      marketplace.interface,
      'InvalidAuctionDuration',
      [MIN_AUCTION_DURATION - 1]
    );

    await (await marketplace.connect(alice).CreateAuction(3, 4, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();
    await expectCustomError(
      () => marketplace.connect(alice).CreateListing.staticCall(3, 4, ethers.parseEther('1.5')),
      marketplace.interface,
      'SaleAlreadyExists',
      [1n]
    );
    await expectCustomError(
      () => marketplace.connect(bob).CancelAuction.staticCall(1),
      marketplace.interface,
      'NotAuctionSeller',
      [bob.address, 1n]
    );
    await expectCustomError(
      () => marketplace.connect(bob).PlaceAuctionBid.staticCall(1, { value: ethers.parseEther('0.9') }),
      marketplace.interface,
      'BidTooLow',
      [ethers.parseEther('1'), ethers.parseEther('0.9')]
    );

    await (await marketplace.connect(bob).PlaceAuctionBid(1, { value: ethers.parseEther('1') })).wait();
    await expectCustomError(
      () => marketplace.connect(alice).CancelAuction.staticCall(1),
      marketplace.interface,
      'AuctionHasExistingBids',
      [1n]
    );
    await expectCustomError(
      () => marketplace.connect(alice).PlaceAuctionBid.staticCall(1, { value: ethers.parseEther('1.05') }),
      marketplace.interface,
      'BidTooLow',
      [ethers.parseEther('1.1'), ethers.parseEther('1.05')]
    );
    await expectCustomError(
      () => marketplace.SettleAuction.staticCall(1),
      marketplace.interface,
      'AuctionNotEnded',
      [1n]
    );
    await expectCustomError(
      () => marketplace.InvalidateStaleAuction.staticCall(1),
      marketplace.interface,
      'SellerNoLongerOwnsChunk',
      [0n]
    );

    await (await claims.connect(alice).ClaimChunk(8, 8, { value: MIN_CHUNK_PRICE })).wait();
    await (await marketplace.connect(alice).CreateAuction(8, 8, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();
    await expectCustomError(
      () => marketplace.connect(bob).CancelAuction.staticCall(2),
      marketplace.interface,
      'NotAuctionSeller',
      [bob.address, 2n]
    );
    await (await marketplace.connect(alice).CancelAuction(2)).wait();
    assert.equal((await marketplace.auctions(2)).active, false);

    await (await claims.connect(alice).ClaimChunk(12, 12, { value: MIN_CHUNK_PRICE })).wait();
    const noBidTokenId = await claims.TokenIdOfChunk(12, 12);
    await (await marketplace.connect(alice).CreateAuction(12, 12, ethers.parseEther('1'), ethers.parseEther('0.1'), 120)).wait();
    await provider.send('evm_increaseTime', [121]);
    await provider.send('evm_mine', []);
    await (await marketplace.SettleAuction(3)).wait();

    const noBidAuction = await marketplace.auctions(3);
    assert.equal(noBidAuction.active, false);
    assert.equal(noBidAuction.settled, true);
    assert.equal(await claims.OwnerOf(12, 12), alice.address);
    const clearedSaleState = await marketplace.GetSaleStateForToken(noBidTokenId);
    assert.equal(clearedSaleState.saleKind, 0n);
    assert.equal(clearedSaleState.active, false);
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

    await expectNetBalanceIncrease(
      provider,
      owner.address,
      seller.connect(owner).WithdrawMarketplaceBalance(owner.address),
      ethers.parseEther('0.475')
    );
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

    await expectNetBalanceIncrease(
      provider,
      owner.address,
      bidder.connect(owner).WithdrawMarketplaceBalance(owner.address),
      ethers.parseEther('1')
    );
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

    await expectNetBalanceIncrease(
      provider,
      owner.address,
      seller.connect(owner).WithdrawMarketplaceBalance(owner.address),
      ethers.parseEther('0.95')
    );
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

    await expectCustomError(
      () => marketplace.connect(carol).PurchaseListing.staticCall(1, { value: ethers.parseEther('0.25') }),
      marketplace.interface,
      'NotRegisteredBuyer',
      [carol.address]
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

    await expectCustomError(
      () => claims.connect(bob).ClaimChunk.staticCall(4, 4),
      claims.interface,
      'IncorrectChunkPurchasePrice',
      [MIN_CHUNK_PRICE, 0n]
    );

    await (await claims.connect(bob).ClaimChunk(4, 4, { value: MIN_CHUNK_PRICE })).wait();
    assert.equal(await claims.OwnerOf(4, 4), bob.address);
    assert.equal(await claims.TokenIdOfChunk(4, 4), tokenId + 1n);
  });
});
