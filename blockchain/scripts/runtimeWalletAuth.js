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

function ensureHex(value, field)
{
  const normalized = ensureString(value, field);
  if (!/^0x[0-9a-fA-F]*$/.test(normalized) || (normalized.length % 2) !== 0)
  {
    fail(`Invalid hex for ${field}.`);
  }
  return normalized;
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
  const mode = ensureString(payload.mode, 'mode').toLowerCase();

  if (mode === 'address')
  {
    const privateKey = ensureString(payload.privateKey, 'privateKey');
    const wallet = new ethers.Wallet(privateKey);
    process.stdout.write(JSON.stringify({ ok: true, address: wallet.address.toLowerCase() }));
    return;
  }

  const messageHex = ensureHex(payload.messageHex, 'messageHex');
  const messageBytes = ethers.getBytes(messageHex);

  if (mode === 'sign')
  {
    const privateKey = ensureString(payload.privateKey, 'privateKey');
    const wallet = new ethers.Wallet(privateKey);
    const signature = await wallet.signMessage(messageBytes);
    process.stdout.write(JSON.stringify({ ok: true, address: wallet.address.toLowerCase(), signature }));
    return;
  }

  if (mode === 'recover')
  {
    const signature = ensureString(payload.signature, 'signature');
    const address = ethers.verifyMessage(messageBytes, signature);
    process.stdout.write(JSON.stringify({ ok: true, address: address.toLowerCase() }));
    return;
  }

  fail(`Unsupported mode: ${mode}`);
}

main().catch((error) =>
{
  const message = error && error.message ? error.message : String(error);
  process.stdout.write(JSON.stringify({ ok: false, error: message }));
  process.exit(1);
});
