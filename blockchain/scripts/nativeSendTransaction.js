const fs = require('fs');
const path = require('path');
const { ethers } = require('ethers');

function fail(message)
{
  process.stdout.write(JSON.stringify({ ok: false, error: message }));
  process.exit(1);
}

function ensureString(value, field)
{
  if (typeof value !== 'string' || value.trim() === '')
  {
    fail(`Missing or invalid ${field}.`);
  }
  return value.trim();
}

async function main()
{
  const payloadPath = process.argv[2];
  if (!payloadPath)
  {
    fail('Missing payload path argument.');
  }

  const raw = fs.readFileSync(path.resolve(payloadPath), 'utf8');
  const payload = JSON.parse(raw);

  const rpcUrl = ensureString(payload.rpcUrl, 'rpcUrl');
  const privateKey = ensureString(payload.privateKey, 'privateKey');
  const to = ensureString(payload.to, 'to');
  const data = ensureString(payload.data, 'data');
  const value = typeof payload.value === 'string' && payload.value.trim() !== ''
    ? payload.value.trim()
    : '0x0';

  const provider = new ethers.JsonRpcProvider(rpcUrl);
  const wallet = new ethers.Wallet(privateKey, provider);
  const nonce = await provider.getTransactionCount(wallet.address, 'pending');
  const feeData = await provider.getFeeData();

  const txRequest = {
    to,
    data,
    value,
    nonce
  };

  const network = await provider.getNetwork();
  txRequest.chainId = Number(network.chainId);

  if (feeData.maxFeePerGas != null && feeData.maxPriorityFeePerGas != null)
  {
    txRequest.maxFeePerGas = feeData.maxFeePerGas;
    txRequest.maxPriorityFeePerGas = feeData.maxPriorityFeePerGas;
    txRequest.type = 2;
  }
  else if (feeData.gasPrice != null)
  {
    txRequest.gasPrice = feeData.gasPrice;
  }

  txRequest.gasLimit = await provider.estimateGas({
    from: wallet.address,
    to,
    data,
    value
  });

  const signed = await wallet.signTransaction(txRequest);
  const response = await provider.broadcastTransaction(signed);
  process.stdout.write(JSON.stringify({ ok: true, hash: response.hash }));
}

main().catch((error) =>
{
  const message = error && error.message ? error.message : String(error);
  process.stdout.write(JSON.stringify({ ok: false, error: message }));
  process.exit(1);
});
