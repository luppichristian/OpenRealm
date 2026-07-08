from __future__ import annotations

from pathlib import Path

from scripts.python_helpers import fail, run_command, succeed


SCRIPT_DIR = Path(__file__).resolve().parent


if __name__ == '__main__':
  print('[blockchain] building Solidity artifacts...')
  status = run_command(['npm', 'run', 'build'], cwd=SCRIPT_DIR)
  if status != 0:
    fail('[blockchain] build failed.')
  succeed('[blockchain] build completed successfully.')
