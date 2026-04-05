#!/usr/bin/env bash

set -euo pipefail

target="${1:-build/docs/ja/index.html}"

if [ ! -f "$target" ]; then
  printf 'docs not found: %s\n' "$target" >&2
  exit 1
fi

open_with_macos() {
  open "$target" >/dev/null 2>&1 &
}

open_with_wsl() {
  if command -v wslview >/dev/null 2>&1; then
    wslview "$target" >/dev/null 2>&1 &
    return
  fi

  if command -v powershell.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
    win_target="$(wslpath -w "$target")"
    powershell.exe -NoProfile -Command "Start-Process -FilePath '$win_target'" >/dev/null
    return
  fi

  printf 'docs generated at %s, but no WSL opener was found.\n' "$target" >&2
  return 0
}

open_with_linux() {
  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$target" >/dev/null 2>&1 &
    return
  fi

  printf 'docs generated at %s, but no opener was found.\n' "$target" >&2
}

if [ "$(uname -s)" = "Darwin" ]; then
  open_with_macos
  exit 0
fi

if grep -qi microsoft /proc/version 2>/dev/null; then
  open_with_wsl
  exit 0
fi

open_with_linux
