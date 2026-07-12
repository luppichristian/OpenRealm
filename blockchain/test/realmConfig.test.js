const assert = require('assert');
const path = require('path');
const { loadRealmConfig, parseJsonWithComments, stripJsonComments } = require('../scripts/realmConfig');
const { ORCHESTRATION_PROTOCOL_VERSION } = require('../scripts/protocolVersion');

describe('realm config loader', function ()
{
  it('parses commented realm JSON files used by the repo', function ()
  {
    const realm = loadRealmConfig(path.resolve(__dirname, '..', '..', 'realms', 'test'));

    assert.equal(realm.realmName, 'test');
    assert.equal(realm.blockchain.protocolVersion, ORCHESTRATION_PROTOCOL_VERSION);
    assert.equal(realm.blockchain.rpcUrl, 'http://127.0.0.1:8545');
  });

  it('strips line and block comments without touching comment-like text inside strings', function ()
  {
    const raw = `{
      // top-level comment
      "url": "https://example.invalid//kept",
      /* block comment */
      "note": "/* kept */",
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
    assert.equal(parsed.nested.value, 7);
  });
});
