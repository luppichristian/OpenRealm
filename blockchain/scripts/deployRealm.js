const path = require('path');
const { spawnSync } = require('child_process');
const { loadRealmConfig } = require('./realmConfig');

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

function stripRealmArg(argv)
{
  const filteredArgs = [];
  for (let index = 0; index < argv.length; index += 1)
  {
    const token = argv[index];
    if (token === '--realm')
    {
      index += 1;
      continue;
    }

    filteredArgs.push(token);
  }
  return filteredArgs;
}

function main()
{
  const rawArgs = process.argv.slice(2);
  const args = parseArgs(rawArgs);
  const realm = loadRealmConfig(args.realm || process.env.DEPLOY_REALM);
  const forwardedArgs = stripRealmArg(rawArgs);
  const deployScriptPath = path.resolve(__dirname, 'deploy.js');

  if (!args.rpc && realm.blockchain.rpcUrl)
  {
    forwardedArgs.push('--rpc', realm.blockchain.rpcUrl);
  }

  if (!args.network && realm.realmName)
  {
    forwardedArgs.push('--network', realm.realmName);
  }

  const result = spawnSync(process.execPath, [deployScriptPath, ...forwardedArgs], { stdio: 'inherit' });
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
    console.error('[deploy-realm] failed');
    console.error(error);
    process.exit(1);
  }
}

module.exports = {
  main,
  parseArgs,
  stripRealmArg
};
