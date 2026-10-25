#!/usr/bin/env bash
# Runs every demo/tests/*.gd headless and reports PASS/FAIL per test.
# Usage: ./demo/tests/run_tests.sh   (GODOT=/path/to/godot to override)
set -u
cd "$(dirname "$0")/../.." || exit 1
GODOT="${GODOT:-godot}"
TIMEOUT="$(command -v timeout || command -v gtimeout || echo /opt/homebrew/bin/timeout)"
mkdir -p demo/tests/logs
status=0
for test in demo/tests/*.gd; do
  name="$(basename "$test" .gd)"
  log="demo/tests/logs/$name.log"
  "$TIMEOUT" --kill-after=10 120 "$GODOT" --headless --path demo -s "tests/$name.gd" >"$log" 2>&1
  code=$?
  if [ "$code" -eq 0 ]; then
    echo "PASS $name"
  else
    echo "FAIL $name (exit $code) - see $log"
    status=1
  fi
  grep -h "^RESULT:\|^SKIP:" "$log" || true
done
exit $status
