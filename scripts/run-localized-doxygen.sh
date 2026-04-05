#!/usr/bin/env bash

set -euo pipefail

lang="${1:?usage: scripts/run-localized-doxygen.sh <en|ja> <output-root>}"
output_root="${2:?usage: scripts/run-localized-doxygen.sh <en|ja> <output-root>}"

case "$lang" in
  en)
    mainpage="README.md"
    output_language="English"
    input_paths="src docs/openapi.yaml README.md"
    while IFS= read -r path; do
      input_paths="${input_paths} ${path}"
    done < <(find docs -maxdepth 1 -type f -name '*.md' ! -name '*.ja.md' | sort)
    ;;
  ja)
    mainpage="README.ja.md"
    output_language="Japanese"
    input_paths="src docs/openapi.yaml README.ja.md"
    while IFS= read -r path; do
      input_paths="${input_paths} ${path}"
    done < <(find docs -maxdepth 1 -type f -name '*.ja.md' | sort)
    ;;
  *)
  printf 'unsupported language: %s\n' "$lang" >&2
    exit 1
    ;;
esac

tmpfile="$(mktemp)"
trap 'rm -f "$tmpfile"' EXIT

cat Doxyfile > "$tmpfile"
{
  printf '\nOUTPUT_DIRECTORY = %s\n' "$output_root"
  printf 'HTML_OUTPUT = %s\n' "$lang"
  printf 'INPUT = %s\n' "$input_paths"
  printf 'USE_MDFILE_AS_MAINPAGE = %s\n' "$mainpage"
  printf 'OUTPUT_LANGUAGE = %s\n' "$output_language"
} >> "$tmpfile"

doxygen "$tmpfile"
