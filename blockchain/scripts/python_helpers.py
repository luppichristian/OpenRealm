from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path
from typing import Iterable, Sequence


def resolve_command(name: str) -> str:
  commandPath = shutil.which(name);
  if commandPath is None:
    raise FileNotFoundError(f"Required command '{name}' was not found on PATH.")
  return commandPath


def print_blank_line() -> None:
  print()


def run_command(command: Sequence[str], *, cwd: Path) -> int:
  resolvedCommand = [resolve_command(command[0]), *command[1:]]
  completedProcess = subprocess.run(list(resolvedCommand), cwd=cwd)
  return int(completedProcess.returncode)


def forward_script(scriptPath: Path, forwardedArgs: Iterable[str], *, cwd: Path) -> int:
  command = [resolve_command('node'), str(scriptPath), *forwardedArgs]
  completedProcess = subprocess.run(command, cwd=cwd)
  return int(completedProcess.returncode)


def exit_with_status(status: int) -> None:
  raise SystemExit(int(status))


def fail(message: str) -> None:
  print_blank_line()
  print(message)
  exit_with_status(1)


def succeed(message: str) -> None:
  print_blank_line()
  print(message)
  exit_with_status(0)
