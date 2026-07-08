const path = require('path');
const { spawnSync } = require('child_process');

const defaultPrivateKey = '0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80';
const defaultOwnerAddress = '0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266';

function main()
{
  const privateKey = process.argv[2] || defaultPrivateKey;
  const ownerAddress = process.argv[3] || defaultOwnerAddress;
  const deployScriptPath = path.resolve(__dirname, 'deploy.js');

  console.log('[realms/test] deploying test realm to local Ganache...');
  console.log(`[realms/test] owner=${ownerAddress}`);
  console.log('');

  const result = spawnSync(
    process.execPath,
    [deployScriptPath, '--private-key', privateKey, '--owner', ownerAddress],
    { stdio: 'inherit' }
  );

  if (result.error)
  {
    throw result.error;
  }

  if (result.status !== 0)
  {
    console.log('');
    console.error('[realms/test] local deployment failed.');
    process.exit(result.status || 1);
  }

  console.log('');
  console.log('[realms/test] local deployment completed. See ../../blockchain/deployments/test.json');
}

if (require.main === module)
{
  try
  {
    main();
  }
  catch (error)
  {
    console.error('[realms/test] local deployment failed.');
    console.error(error);
    process.exit(1);
  }
}

module.exports = {
  main
};
