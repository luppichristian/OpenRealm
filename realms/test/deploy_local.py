from __future__ import annotations

import argparse
import sys
from pathlib import Path

BLOCKCHAIN_DIR = Path(__file__).resolve().parents[2] / 'blockchain'
if str(BLOCKCHAIN_DIR) not in sys.path:
  sys.path.insert(0, str(BLOCKCHAIN_DIR))

from scripts.python_helpers import fail, forward_script, succeed


SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_PRIVATE_KEY = '0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80'
DEFAULT_OWNER_ADDRESS = '0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266'


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description='Deploy the test realm to a local Ganache instance.')
  parser.add_argument('private_key', nargs='?', default=DEFAULT_PRIVATE_KEY)
  parser.add_argument('owner_address', nargs='?', default=DEFAULT_OWNER_ADDRESS)
  return parser.parse_args()


if __name__ == '__main__':
  args = parse_args()
  print('[realms/test] deploying test realm to local Ganache...')
  print(f'[realms/test] owner={args.owner_address}')
  print()

  status = forward_script(
    SCRIPT_DIR / 'deploy.js',
    ['--private-key', args.private_key, '--owner', args.owner_address],
    cwd=SCRIPT_DIR
  )
  if status != 0:
    fail('[realms/test] local deployment failed.')
  succeed('[realms/test] local deployment completed. See ../../blockchain/deployments/test.json')
