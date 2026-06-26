const fs = require('fs');
const path = require('path');
const { compileContracts } = require('../test/helpers/compileContracts');

function main()
{
  const compiledContracts = compileContracts();
  const artifactsDir = path.resolve(__dirname, '..', 'artifacts');
  fs.rmSync(artifactsDir, { recursive: true, force: true });
  fs.mkdirSync(artifactsDir, { recursive: true });

  const manifest = {};

  for (const [sourceName, contracts] of Object.entries(compiledContracts))
  {
    for (const [contractName, artifact] of Object.entries(contracts))
    {
      const relativeDir = path.join(artifactsDir, path.dirname(sourceName));
      fs.mkdirSync(relativeDir, { recursive: true });

      const artifactPath = path.join(relativeDir, `${contractName}.json`);
      const payload = {
        contractName,
        sourceName,
        abi: artifact.abi,
        bytecode: `0x${artifact.evm.bytecode.object}`
      };

      fs.writeFileSync(artifactPath, JSON.stringify(payload, null, 2));
      manifest[`${sourceName}:${contractName}`] = path.relative(artifactsDir, artifactPath).split(path.sep).join('/');
      console.log(`[build] wrote ${path.relative(path.resolve(__dirname, '..'), artifactPath).split(path.sep).join('/')}`);
    }
  }

  fs.writeFileSync(path.join(artifactsDir, 'manifest.json'), JSON.stringify(manifest, null, 2));
  console.log(`[build] wrote artifacts/manifest.json (${Object.keys(manifest).length} artifacts)`);
}

main();
