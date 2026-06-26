const fs = require('fs');
const path = require('path');
const solc = require('solc');

function collectSoliditySources(rootDir)
{
  const sources = {};

  function walk(currentDir)
  {
    const entries = fs.readdirSync(currentDir, { withFileTypes: true });
    for (const entry of entries)
    {
      const fullPath = path.join(currentDir, entry.name);
      if (entry.isDirectory())
      {
        walk(fullPath);
        continue;
      }

      if (!entry.isFile() || !entry.name.endsWith('.sol'))
      {
        continue;
      }

      const relativePath = path.relative(rootDir, fullPath).split(path.sep).join('/');
      sources[relativePath] = { content: fs.readFileSync(fullPath, 'utf8') };
    }
  }

  walk(rootDir);
  return sources;
}

function compileContracts()
{
  const contractsDir = path.resolve(__dirname, '..', '..', 'contracts');
  const input = {
    language: 'Solidity',
    sources: collectSoliditySources(contractsDir),
    settings: {
      optimizer: {
        enabled: true,
        runs: 200
      },
      outputSelection: {
        '*': {
          '*': ['abi', 'evm.bytecode.object']
        }
      }
    }
  };

  const output = JSON.parse(solc.compile(JSON.stringify(input)));
  if (output.errors)
  {
    const errors = output.errors.filter((entry) => entry.severity === 'error');
    if (errors.length > 0)
    {
      throw new Error(errors.map((entry) => entry.formattedMessage).join('\n\n'));
    }
  }

  return output.contracts;
}

function getContractArtifact(compiledContracts, sourceName, contractName)
{
  const sourceContracts = compiledContracts[sourceName];
  if (!sourceContracts || !sourceContracts[contractName])
  {
    throw new Error(`Missing artifact for ${sourceName}:${contractName}`);
  }

  const artifact = sourceContracts[contractName];
  return {
    abi: artifact.abi,
    bytecode: `0x${artifact.evm.bytecode.object}`
  };
}

module.exports = {
  compileContracts,
  getContractArtifact
};
