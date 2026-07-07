const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { ORCHESTRATION_PROTOCOL_VERSION } = require('../../blockchain/scripts/protocolVersion');

function loadTestRealmConfig()
{
  const realmPath = path.resolve(__dirname, 'realm.json');
  const realmConfig = JSON.parse(fs.readFileSync(realmPath, 'utf8'));
  if (!realmConfig || typeof realmConfig !== 'object')
  {
    throw new Error('Invalid realms/test/realm.json payload.');
  }

  const blockchain = realmConfig.blockchain;
  if (!blockchain || typeof blockchain !== 'object')
  {
    throw new Error('Missing blockchain config in realms/test/realm.json.');
  }

  if (!blockchain.rpcUrl)
  {
    throw new Error('Missing blockchain.rpcUrl in realms/test/realm.json.');
  }

  if (blockchain.protocolVersion !== ORCHESTRATION_PROTOCOL_VERSION)
  {
    throw new Error(
      `realms/test/realm.json protocolVersion ${blockchain.protocolVersion} does not match orchestration protocol version ${ORCHESTRATION_PROTOCOL_VERSION}.`
    );
  }

  return {
    realmName: realmConfig.name || 'test',
    rpcUrl: blockchain.rpcUrl
  };
}

function main()
{
  const { realmName, rpcUrl } = loadTestRealmConfig();
  const deployScriptPath = path.resolve(__dirname, '..', '..', 'blockchain', 'scripts', 'deploy.js');
  const forwardedArgs = process.argv.slice(2);
  const result = spawnSync(
    process.execPath,
    [deployScriptPath, '--rpc', rpcUrl, '--network', realmName, ...forwardedArgs],
    { stdio: 'inherit' }
  );

  if (result.error)
  {
    throw result.error;
  }

  process.exit(result.status || 0);
}

if (require.main === module)
{
  main();
}

module.exports = {
  loadTestRealmConfig,
  main
};
