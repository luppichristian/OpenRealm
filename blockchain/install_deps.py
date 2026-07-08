from __future__ import annotations

from pathlib import Path

from scripts.python_helpers import fail, print_blank_line, run_command, succeed


SCRIPT_DIR = Path(__file__).resolve().parent


if __name__ == '__main__':
  print('[blockchain] installing npm dependencies...')
  status = run_command(['npm', 'install'], cwd=SCRIPT_DIR)
  if status != 0:
    fail('[blockchain] npm install failed.')
  succeed('[blockchain] dependencies installed successfully.')
