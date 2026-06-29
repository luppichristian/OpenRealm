const fs = require('fs');
const path = require('path');
const { ethers } = require('ethers');
const { compileContracts, getContractArtifact } = require('../test/helpers/compileContracts');

function parseArgs(argv)
{
  const args = {};
  for (let index = 0; index < argv.length; index += 1)
  {
    const token = argv[index];
    if (!token.startsWith('--'))
    {
      continue;
    }

    const key = token.slice(2);
    const value = argv[index + 1] && !argv[index + 1].startsWith('--') ? argv[++index] : 'true';
    args[key] = value;
  }
  return args;
}

async function deployContract(compiledContracts, signer, contractName, args = [], overrides = {})
{
  const artifact = getContractArtifact(compiledContracts, `${contractName}.sol`, contractName);
  const factory = new ethers.ContractFactory(artifact.abi, artifact.bytecode, signer);
  const contract = await factory.deploy(...args, overrides);
  await contract.waitForDeployment();
  return contract;
}

function ensureRequired(value, message)
{
  if (!value)
  {
    throw new Error(message);
  }

  return value;
}

async function main()
{
  const args = parseArgs(process.argv.slice(2));
  const rpcUrl = args.rpc || process.env.RPC_URL;
  const privateKey = args['private-key'] || process.env.PRIVATE_KEY;
  const networkName = args.network || process.env.DEPLOY_NETWORK || 'custom';
  const ownerAddressArg = args.owner || process.env.OWNER_ADDRESS;
  const minChunkCoord = Number(args['min-chunk-coord'] || process.env.MIN_CHUNK_COORD || '-30000');
  const maxChunkCoord = Number(args['max-chunk-coord'] || process.env.MAX_CHUNK_COORD || '30000');
  const minChunkPrice = BigInt(args['min-chunk-price-wei'] || process.env.MIN_CHUNK_PRICE_WEI || ethers.parseEther('0.01').toString());
  const maxFeeBps = Number(args['max-fee-bps'] || process.env.MAX_FEE_BPS || '2500');
  const minAuctionDuration = Number(args['min-auction-duration-seconds'] || process.env.MIN_AUCTION_DURATION_SECONDS || '60');
  const feeBps = Number(args['fee-bps'] || process.env.FEE_BPS || '500');

  ensureRequired(rpcUrl, 'Missing RPC URL. Pass --rpc <url> or set RPC_URL.');
  ensureRequired(privateKey, 'Missing deployer private key. Pass --private-key <key> or set PRIVATE_KEY.');

  const provider = new ethers.JsonRpcProvider(rpcUrl);
  const wallet = new ethers.Wallet(privateKey, provider);
  const ownerAddress = ownerAddressArg || wallet.address;
  const compiledContracts = compileContracts();
  let nonce = await provider.getTransactionCount(wallet.address, 'latest');

  console.log(`[deploy] network=${networkName}`);
  console.log(`[deploy] deployer=${wallet.address}`);
  console.log(`[deploy] owner=${ownerAddress}`);
  console.log(`[deploy] min_chunk_coord=${minChunkCoord}`);
  console.log(`[deploy] max_chunk_coord=${maxChunkCoord}`);
  console.log(`[deploy] min_chunk_price_wei=${minChunkPrice}`);
  console.log(`[deploy] max_fee_bps=${maxFeeBps}`);
  console.log(`[deploy] min_auction_duration_seconds=${minAuctionDuration}`);
  console.log(`[deploy] fee_bps=${feeBps}`);

  const registry = await deployContract(compiledContracts, wallet, 'PlayerRegistry', [ownerAddress], { nonce });
  nonce += 1;
  console.log(`[deploy] registry=${await registry.getAddress()}`);

  const globalParams = await deployContract(
    compiledContracts,
    wallet,
    'GlobalParams',
    [minChunkCoord, maxChunkCoord, minChunkPrice, maxFeeBps, minAuctionDuration],
    { nonce }
  );
  nonce += 1;
  console.log(`[deploy] global_params=${await globalParams.getAddress()}`);

  const claims = await deployContract(
    compiledContracts,
    wallet,
    'ChunkClaims',
    [ownerAddress, await registry.getAddress(), await globalParams.getAddress()],
    { nonce }
  );
  nonce += 1;
  console.log(`[deploy] claims=${await claims.getAddress()}`);

  const marketplace = await deployContract(
    compiledContracts,
    wallet,
    'Marketplace',
    [ownerAddress, await claims.getAddress(), await globalParams.getAddress(), feeBps],
    { nonce }
  );
  nonce += 1;
  console.log(`[deploy] marketplace=${await marketplace.getAddress()}`);

  const setMarketplaceTx = await claims.connect(wallet).SetMarketplace(await marketplace.getAddress(), { nonce });
  await setMarketplaceTx.wait();
  console.log(`[deploy] marketplace linked in tx=${setMarketplaceTx.hash}`);

  const chainId = Number((await provider.getNetwork()).chainId);
  const deploymentRecord = {
    network: networkName,
    chainId,
    ownerAddress,
    deployerAddress: wallet.address,
    globalParams: {
      minChunkCoord,
      maxChunkCoord,
      minChunkPriceWei: minChunkPrice.toString(),
      maxFeeBps,
      minAuctionDurationSeconds: minAuctionDuration
    },
    feeBps,
    deployedAt: new Date().toISOString(),
    contracts: {
      PlayerRegistry: await registry.getAddress(),
      GlobalParams: await globalParams.getAddress(),
      ChunkClaims: await claims.getAddress(),
      Marketplace: await marketplace.getAddress()
    }
  };

  const deploymentsDir = path.resolve(__dirname, '..', 'deployments');
  fs.mkdirSync(deploymentsDir, { recursive: true });
  const outputPath = path.join(deploymentsDir, `${networkName}.json`);
  fs.writeFileSync(outputPath, JSON.stringify(deploymentRecord, null, 2));
  console.log(`[deploy] wrote ${path.relative(path.resolve(__dirname, '..'), outputPath).split(path.sep).join('/')}`);
}

main().catch((error) =>
{
  console.error('[deploy] failed');
  console.error(error);
  process.exit(1);
});
