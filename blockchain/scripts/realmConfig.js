const fs = require('fs');
const path = require('path');
const { ORCHESTRATION_PROTOCOL_VERSION } = require('./protocolVersion');

function resolveRealmJsonPath(realmArg)
{
  if (!realmArg)
  {
    throw new Error('Missing realm path or realm name. Pass --realm <name-or-path>.');
  }

  const repoRoot = path.resolve(__dirname, '..', '..');
  const normalizedArg = realmArg.trim();
  const candidatePaths = [];

  if (path.isAbsolute(normalizedArg))
  {
    candidatePaths.push(normalizedArg);
  }
  else if (normalizedArg.endsWith('.json'))
  {
    candidatePaths.push(path.resolve(process.cwd(), normalizedArg));
    candidatePaths.push(path.resolve(repoRoot, normalizedArg));
  }
  else if (normalizedArg.includes('/') || normalizedArg.includes('\\'))
  {
    candidatePaths.push(path.resolve(process.cwd(), normalizedArg, 'realm.json'));
    candidatePaths.push(path.resolve(repoRoot, normalizedArg, 'realm.json'));
  }
  else
  {
    candidatePaths.push(path.resolve(repoRoot, 'realms', normalizedArg, 'realm.json'));
  }

  for (const candidatePath of candidatePaths)
  {
    const statsPath = candidatePath.endsWith('.json') ? candidatePath : path.join(candidatePath, 'realm.json');
    if (fs.existsSync(statsPath))
    {
      return statsPath;
    }
  }

  throw new Error(`Could not resolve realm config for '${realmArg}'.`);
}

function loadRealmConfig(realmArg)
{
  const realmJsonPath = resolveRealmJsonPath(realmArg);
  const realmDir = path.dirname(realmJsonPath);
  const realmConfig = JSON.parse(fs.readFileSync(realmJsonPath, 'utf8'));
  if (!realmConfig || typeof realmConfig !== 'object')
  {
    throw new Error(`Invalid realm config payload at ${realmJsonPath}.`);
  }

  const blockchain = realmConfig.blockchain;
  if (!blockchain || typeof blockchain !== 'object')
  {
    throw new Error(`Missing blockchain config in ${realmJsonPath}.`);
  }

  if (blockchain.protocolVersion !== ORCHESTRATION_PROTOCOL_VERSION)
  {
    throw new Error(
      `${realmJsonPath} protocolVersion ${blockchain.protocolVersion} does not match orchestration protocol version ${ORCHESTRATION_PROTOCOL_VERSION}.`
    );
  }

  return {
    realmArg,
    realmDir,
    realmJsonPath,
    realmConfig,
    realmName: realmConfig.name || path.basename(realmDir),
    blockchain
  };
}

module.exports = {
  loadRealmConfig,
  resolveRealmJsonPath
};
