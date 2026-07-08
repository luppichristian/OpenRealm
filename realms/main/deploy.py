from __future__ import annotations

import sys
from pathlib import Path

BLOCKCHAIN_DIR = Path(__file__).resolve().parents[2] / 'blockchain'
if str(BLOCKCHAIN_DIR) not in sys.path:
  sys.path.insert(0, str(BLOCKCHAIN_DIR))

from scripts.python_helpers import exit_with_status, forward_script


SCRIPT_DIR = Path(__file__).resolve().parent


if __name__ == '__main__':
  print('[realms/main] deploying main realm using realms/main/realm.json...')
  status = forward_script(SCRIPT_DIR / 'deploy.js', sys.argv[1:], cwd=SCRIPT_DIR)
  exit_with_status(status)
