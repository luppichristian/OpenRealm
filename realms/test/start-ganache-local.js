const { spawnSync } = require('child_process');

const mnemonic = 'test test test test test test test test test test test junk';

function main()
{
  console.log('[realms/test] starting local Ganache node for the test realm...');
  console.log('[realms/test] chainId=31337');
  console.log('[realms/test] rpcUrl=http://127.0.0.1:8545');
  console.log(`[realms/test] mnemonic=${mnemonic}`);
  console.log('');

  const command = [
    'npx ganache',
    `--wallet.mnemonic "${mnemonic}"`,
    '--wallet.totalAccounts 10',
    '--chain.chainId 31337',
    '--server.port 8545'
  ].join(' ');

  const result = spawnSync(command, {
    stdio: 'inherit',
    shell: true
  });

  if (result.error)
  {
    throw result.error;
  }

  process.exit(result.status || 0);
}

if (require.main === module)
{
  try
  {
    main();
  }
  catch (error)
  {
    console.error('[realms/test] failed to start local Ganache.');
    console.error(error);
    process.exit(1);
  }
}

module.exports = {
  main
};
