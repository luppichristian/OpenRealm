const path = require('path');
const { spawnSync } = require('child_process');

function main()
{
  const deployRealmScriptPath = path.resolve(__dirname, '..', '..', 'blockchain', 'scripts', 'deployRealm.js');
  const forwardedArgs = process.argv.slice(2);
  const result = spawnSync(
    process.execPath,
    [deployRealmScriptPath, '--realm', __dirname, ...forwardedArgs],
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
  main
};
