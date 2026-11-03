#!/usr/bin/env bash
#
# mayhem/test.sh — runs the known-answer probe that mayhem/build.sh built (normal flags, no
# sanitizers) over the real fs2open table parser, text/encoding helpers, MD5/base64/FNV hashes and
# the FS1/FS2 file scrambler, and compares every printed value with a golden constant. The upstream
# gtest suite needs the whole engine (SDL, OpenAL, FFmpeg, ...) and is not buildable here.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

PROBE="$SRC/kat_probe"
if [ ! -x "$PROBE" ]; then
  echo "kat_probe missing: mayhem/build.sh did not build it" >&2
  emit_ctrf "fs2open-kat" 0 1 0
  exit 1
fi

actual="$("$PROBE" 2>&1)"

# Golden values. md5/base64/fnv1a come from RFC 1321 / RFC 4648 / the FNV reference vectors;
# the rest are what the engine's parser documents for these inputs.
read -r -d '' EXPECTED <<'GOLDEN'
parse.name=Test Ship
parse.count=42
parse.ratio=3.50
parse.pos=1.00,-2.50,3.25
parse.list=5,19
parse.after=7
parse.quoted="hello world"
parse.end=1
md5.abc=900150983cd24fb0d6963f7d28e17f72
md5.empty=d41d8cd98f00b204e9800998ecf8427e
b64.enc=TWFu
b64.dec=foobar
fnv1a.a=e40c292c
foreign.ss=ss
conv.l1_to_u8=1,636166c3a9
cp.count=5
ascii.only=1,0
enc.utf16le=2
enc.utf8bom=1,1
ws.trim=[padded text]
dbl.quote=aa"bb"
hash.cut=keep
crypt.mode0=1,FreeSpace 2 type encryption,43,$Name: Rapier|+Cost: 1200 ; sneaky comment|
crypt.mode1=1,FreeSpace 1 type encryption, 7-bit,43,$Name: Rapier|+Cost: 1200 ; sneaky comment|
crypt.mode2=1,FreeSpace 1 type encryption, 8-bit,43,$Name: Rapier|+Cost: 1200 ; sneaky comment|
GOLDEN

passed=0; failed=0
while IFS= read -r line; do
  [ -n "$line" ] || continue
  if printf '%s\n' "$actual" | grep -qFx -- "$line"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL: expected line not produced: $line" >&2
  fi
done <<< "$EXPECTED"

emit_ctrf "fs2open-kat" "$passed" "$failed" 0
