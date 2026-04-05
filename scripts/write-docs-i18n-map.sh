#!/usr/bin/env bash

set -euo pipefail

outdir="${1:-build/docs/html}"
outfile="${outdir}/docs-i18n-map.js"

if [ ! -d "$outdir" ]; then
  printf 'docs output directory not found: %s\n' "$outdir" >&2
  exit 1
fi

{
  printf 'window.DOCS_I18N_MAP = Object.freeze({\n'

  first=1
  while IFS= read -r path; do
    file="$(basename "$path")"
    peer="${file/_8ja/}"

    if [ "$peer" = "$file" ] || [ ! -f "$outdir/$peer" ]; then
      continue
    fi

    if [ "$first" -eq 0 ]; then
      printf ',\n'
    fi

    printf '  "%s": "%s"' "$peer" "$file"
    printf ',\n  "%s": "%s"' "$file" "$peer"
    first=0
  done < <(find "$outdir" -maxdepth 1 -type f -name '*_8ja*.html' | sort)

  printf '\n});\n'
} > "$outfile"
