#!/usr/bin/env bash

set -euo pipefail

first_dir="${1:?usage: scripts/write-docs-i18n-map.sh <first-dir> <second-dir>}"
second_dir="${2:?usage: scripts/write-docs-i18n-map.sh <first-dir> <second-dir>}"

python3 - "$first_dir" "$second_dir" <<'PY'
from __future__ import annotations

import json
import sys
from pathlib import Path


def normalize(name: str) -> str:
    return name.replace("_8ja", "")


def write_map(outdir: Path, peerdir: Path) -> None:
    peer_lookup = {normalize(path.name): path.name for path in peerdir.glob("*.html")}
    mapping: dict[str, str] = {}

    for path in outdir.glob("*.html"):
        peer_name = peer_lookup.get(normalize(path.name))
        if peer_name is not None:
            mapping[path.name] = f"../{peerdir.name}/{peer_name}"

    payload = "window.DOCS_I18N_MAP = Object.freeze(" + json.dumps(
        mapping,
        ensure_ascii=False,
        indent=2,
        sort_keys=True,
    ) + ");\n"
    (outdir / "docs-i18n-map.js").write_text(payload, encoding="utf-8")


first = Path(sys.argv[1])
second = Path(sys.argv[2])

if not first.is_dir() or not second.is_dir():
    raise SystemExit(f"docs output directories not found: {first} {second}")

write_map(first, second)
write_map(second, first)
PY
