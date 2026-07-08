from __future__ import annotations

import sys
from pathlib import Path

BLOCKCHAIN_DIR = Path(__file__).resolve().parents[2] / 'blockchain'
if str(BLOCKCHAIN_DIR) not in sys.path:
  sys.path.insert(0, str(BLOCKCHAIN_DIR))

from scripts.python_helpers import exit_with_status, run_command


SCRIPT_DIR = Path(__file__).resolve().parent
MNEMONIC = 'test test test test test test test test test test test junk'


if __name__ == '__main__':
  print('[realms/test] starting local Ganache node for the test realm...')
  print('[realms/test] chainId=31337')
  print('[realms/test] rpcUrl=http://127.0.0.1:8545')
  print(f'[realms/test] mnemonic={MNEMONIC}')
  print()

  status = run_command([
    'npx',
    'ganache',
    '--wallet.mnemonic',
    MNEMONIC,
    '--wallet.totalAccounts',
    '10',
    '--chain.chainId',
    '31337',
    '--server.port',
    '8545'
  ], cwd=SCRIPT_DIR)
  exit_with_status(status)
