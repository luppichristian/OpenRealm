const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  loadRealmConfig,
  parseJsonWithComments,
  resolveRealmJsonPath,
  stripJsonComments
} = require('../scripts/realmConfig');
const { ORCHESTRATION_PROTOCOL_VERSION } = require('../scripts/protocolVersion');

describe('realm config loader', function ()
{
  function withTempRealmDir(configContent, callback)
  {
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'openrealm-realm-config-'));
    try
    {
      const realmDir = path.join(tempDir, 'temp-realm');
      fs.mkdirSync(realmDir, { recursive: true });
      const realmJsonPath = path.join(realmDir, 'realm.json');
      fs.writeFileSync(realmJsonPath, configContent);
      return callback({ tempDir, realmDir, realmJsonPath });
    }
    finally
    {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  }

  it('parses commented realm JSON files through name, directory, and json-path resolution', function ()
  {
    const expectedPath = path.resolve(__dirname, '..', '..', 'realms', 'test', 'realm.json');
    const byName = loadRealmConfig('test');
    const byDirectory = loadRealmConfig(path.resolve(__dirname, '..', '..', 'realms', 'test'));
    const byJsonPath = loadRealmConfig('realms/test/realm.json');

    for (const realm of [byName, byDirectory, byJsonPath])
    {
      assert.equal(realm.realmName, 'test');
      assert.equal(realm.realmJsonPath, expectedPath);
      assert.equal(realm.realmDir, path.dirname(expectedPath));
      assert.equal(realm.blockchain.protocolVersion, ORCHESTRATION_PROTOCOL_VERSION);
      assert.equal(realm.blockchain.rpcUrl, 'http://127.0.0.1:8545');
      assert.equal(realm.blockchain.connectionTimeoutSeconds, 1);
      assert.equal(realm.blockchain.readTimeoutSeconds, 1);
      assert.equal(realm.blockchain.writeTimeoutSeconds, 1);
      assert.equal(realm.realmConfig.name, 'test');
    }

    assert.equal(resolveRealmJsonPath('test'), expectedPath);
    assert.equal(resolveRealmJsonPath(path.resolve(__dirname, '..', '..', 'realms', 'test')), expectedPath);
    assert.equal(resolveRealmJsonPath('realms/test/realm.json'), expectedPath);
  });

  it('strips comments without touching escaped or comment-like text inside strings', function ()
  {
    const raw = String.raw`{
      // top-level comment
      "url": "https://example.invalid//kept",
      /* block comment */
      "note": "/* kept */",
      "quoted": "He said: \"// still kept\"",
      "path": "C:\\temp\\/*literal*/",
      "nested": {
        "value": 7 // trailing comment
      }
    }`;

    const stripped = stripJsonComments(raw);
    const parsed = parseJsonWithComments(raw, 'inline-test.json');

    assert.ok(!stripped.includes('top-level comment'));
    assert.ok(!stripped.includes('block comment'));
    assert.equal(parsed.url, 'https://example.invalid//kept');
    assert.equal(parsed.note, '/* kept */');
    assert.equal(parsed.quoted, 'He said: "// still kept"');
    assert.equal(parsed.path, 'C:\\temp\\/*literal*/');
    assert.equal(parsed.nested.value, 7);
  });

  it('wraps JSON parse failures with the source file path', function ()
  {
    assert.throws(
      () => parseJsonWithComments('{"broken": }', 'broken-config.json'),
      /Failed to parse JSON config at broken-config\.json:/
    );
  });

  it('rejects realm configs with a protocol-version mismatch', function ()
  {
    withTempRealmDir(`{
      "name": "temp",
      "blockchain": {
        "protocolVersion": ${ORCHESTRATION_PROTOCOL_VERSION + 1},
        "rpcUrl": "http://127.0.0.1:8545"
      }
    }`, ({ realmDir }) =>
    {
      assert.throws(
        () => loadRealmConfig(realmDir),
        new RegExp(`protocolVersion ${ORCHESTRATION_PROTOCOL_VERSION + 1} does not match orchestration protocol version ${ORCHESTRATION_PROTOCOL_VERSION}`)
      );
    });
  });

  it('rejects missing blockchain config objects and unresolved realm paths', function ()
  {
    withTempRealmDir(`{
      "name": "temp"
    }`, ({ realmDir }) =>
    {
      assert.throws(
        () => loadRealmConfig(realmDir),
        /Missing blockchain config/
      );
    });

    assert.throws(
      () => loadRealmConfig('realm-does-not-exist'),
      /Could not resolve realm config for 'realm-does-not-exist'/
    );
    assert.throws(
      () => resolveRealmJsonPath(''),
      /Missing realm path or realm name/
    );
  });
});
