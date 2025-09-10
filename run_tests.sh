#!/usr/bin/env bash
set -euo pipefail

# Repo layout
ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
LLVM_BIN_DEFAULT="/opt/homebrew/opt/llvm/bin"
LLVM_BIN="${LLVM_BIN:-$LLVM_BIN_DEFAULT}"
TEST_DIR="${TEST_DIR:-${ROOT_DIR}/tests}"
PASS_PLUGIN="${PASS_PLUGIN:-${ROOT_DIR}/build/LISHEPass.dylib}"
OPT_PIPELINE_DEFAULT="mem2reg,lishe"
OPT_PIPELINE="$OPT_PIPELINE_DEFAULT"

usage() {
  cat <<EOF
Usage: $0 [--raw] [--pipeline "<opt-passes>"] [--llvm-bin <path>]

Runs LISHE pass on all C tests in ${TEST_DIR}.
Produces *_before.ll and *_after.ll in the same directory.

Options:
  --raw                 Use only 'lishe' (skip mem2reg).
  --pipeline "<passes>" Explicit -passes string (default: "$OPT_PIPELINE_DEFAULT").
  --llvm-bin <path>     Path to LLVM bin dir (default: "$LLVM_BIN_DEFAULT").
  -h, --help            Show this help.

Env overrides:
  LLVM_BIN, TEST_DIR, PASS_PLUGIN
EOF
}

# Parse args
while [ $# -gt 0 ]; do
  case "$1" in
    --raw) OPT_PIPELINE="lishe"; shift ;;
    --pipeline) OPT_PIPELINE="$2"; shift 2 ;;
    --llvm-bin) LLVM_BIN="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1"; usage; exit 1 ;;
  esac
done

CLANG="${LLVM_BIN}/clang"
OPT="${LLVM_BIN}/opt"

# Checks
[ -x "$CLANG" ] || { echo "clang not found at: $CLANG"; exit 1; }
[ -x "$OPT"   ] || { echo "opt not found at:   $OPT"; exit 1; }
[ -f "$PASS_PLUGIN" ] || { echo "Pass plugin not found: $PASS_PLUGIN"; exit 1; }
[ -d "$TEST_DIR" ] || { echo "Tests dir not found: $TEST_DIR"; exit 1; }

echo "== LLVM bin:      $LLVM_BIN"
echo "== Passes:        $OPT_PIPELINE"
echo "== Tests dir:     $TEST_DIR"
echo "== Pass plugin:   $PASS_PLUGIN"
echo

# Collect tests 
TESTS_FOUND=0
for SRC in $(ls -1 "${TEST_DIR}"/*.c 2>/dev/null | LC_ALL=C sort); do
  TESTS_FOUND=1
  base="$(basename "$SRC" .c)"
  BEFORE_LL="${TEST_DIR}/${base}_before.ll"
  AFTER_LL="${TEST_DIR}/${base}_after.ll"

  echo "--> Building unoptimized IR for ${base}"
  "$CLANG" -S -emit-llvm -O0 \
    -Xclang -disable-llvm-passes \
    -Xclang -disable-O0-optnone \
    -o "$BEFORE_LL" "$SRC"

  echo "    Running opt passes: $OPT_PIPELINE"
  "$OPT" -load-pass-plugin "$PASS_PLUGIN" \
        -passes="$OPT_PIPELINE" \
        -S "$BEFORE_LL" -o "$AFTER_LL"
done

if [ "$TESTS_FOUND" -eq 0 ]; then
  echo "No .c tests found in ${TEST_DIR}"
  exit 1
fi

echo
echo "Done. Generated *_before.ll and *_after.ll in ${TEST_DIR}."
