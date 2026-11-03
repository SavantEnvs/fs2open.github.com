#!/usr/bin/env bash
#
# mayhem/build.sh — builds the fs2open harnesses, their standalone reproducers and the KAT probe.
#
# A full CMake build of the engine is not practical here (the libRocket submodule is empty and the
# engine needs SDL3/OpenAL/FFmpeg/Vulkan across ~640 TUs), so the self-contained parse/text/crypto
# layer is compiled straight from the upstream sources with clang. mayhem/stubs.cpp supplies the
# few engine globals that layer references; no upstream file is modified.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OUT="$SRC/mayhem/out"
GEN="$OUT/gen"
mkdir -p "$GEN"

# Generated headers the engine expects from its CMake configure step.
cat > "$GEN/project.h" <<'H'
#ifndef __PROJECT_H
#define __PROJECT_H
#define FS_VERSION_MAJOR 26
#define FS_VERSION_MINOR 1
#define FS_VERSION_BUILD 0
#define FS_VERSION_REVIS 0
#define FS_VERSION_REVIS_STR "0"
#define FS_VERSION_HAS_REVIS 0
#define FS_VERSION_FULL "26.1.0"
#define FS_PRODUCT_VERSION "26.1.0"
#endif
H
cat > "$GEN/platformChecks.h" <<'H'
#ifndef __PLATFORM_CHECKS_H__
#define __PLATFORM_CHECKS_H__
#pragma once
#define SCP_HAVE_EXECINFO_H
#define SCP_HAVE_CXXAPI_H
#define SCP_HAVE_MAX_ALIGN_T
#define SCP_HAVE_STD_MAX_ALIGN_T
#define IS_64BIT 1
#define IS_ARM64 0
#define IS_RISCV 0
#define SCP_HAVE_STRCASECMP 1
#define SCP_HAVE_STRNCASECMP 1
#define SCP_HAVE__STRICMP 0
#define SCP_HAVE__STRNICMP 0
#define SCP_HAVE_STRLWR 0
#define SCP_HAVE_STRINGS_H 1
#define HAVE_CHAR32_T 1
#define HAVE_UNICODE_CHAR_LITERAL 1
#define SCP_HAVE_SNPRINTF 1
#define SCP_HAVE__SNPRINTF 0
#endif
H

INC=(-I"$GEN" -I"$SRC/code" -I"$SRC/freespace2" -I"$SRC/lib" -I"$SRC/lib/lua" -I"$SRC/lib/utfcpp"
     -I"$SRC/lib/lz4" -I"$SRC/lib/md5" -I"$SRC/lib/libpng" -I"$SRC/lib/libjpeg" -I"$SRC/lib/zlib"
     $(pkg-config --cflags sdl3))
CXXSTD=(-std=c++17 -fsigned-char -DNDEBUG -w)

ENGINE_SRCS=(
  code/parse/parselo.cpp code/parse/encrypt.cpp code/parse/md5_hash.cpp
  code/utils/base64.cpp code/utils/encoding.cpp code/utils/string_utils.cpp code/utils/unicode.cpp
  code/utils/Random.cpp code/utils/HeapAllocator.cpp
  code/globalincs/safe_strings.cpp code/globalincs/vmallocator.cpp
  lib/md5/md5.cpp
)
SUPPORT_SRCS=(mayhem/stubs.cpp)
HARNESSES=(fuzz_parselo fuzz_text fuzz_encrypt)

# compile_set <flavor> <flags...> : objects land in $OUT/<flavor>/
#   fuzz       sanitized + libFuzzer coverage instrumentation  (fuzz binaries)
#   standalone sanitized, no libFuzzer instrumentation          (run-once reproducers)
#   normal     project flags, no sanitizers                     (KAT probe / oracle)
compile_set() {
  local flavor="$1"; shift
  local flags=("$@")
  mkdir -p "$OUT/$flavor"
  local f o
  for f in "${ENGINE_SRCS[@]}" "${SUPPORT_SRCS[@]}"; do
    o="$OUT/$flavor/$(echo "$f" | tr / _).o"
    local extra=()
    # encrypt.cpp left-shifts negative chars all over (benign, implementation-defined in practice);
    # relax only that one check, only on this one file.
    case "$f" in */encrypt.cpp) [ "$flavor" = normal ] || extra=(-fno-sanitize=shift-base);; esac
    "$CXX" "${CXXSTD[@]}" "${flags[@]}" "${extra[@]}" "${INC[@]}" -c "$f" -o "$o" &
    while [ "$(jobs -rp | wc -l)" -ge "$MAYHEM_JOBS" ]; do wait -n || exit 1; done
  done
  while [ "$(jobs -rp | wc -l)" -gt 0 ]; do wait -n || exit 1; done
}

lib_objs() { ls "$OUT/$1"/*.o | grep -v '/harness_'; }

# 1) project code with sanitizers + libFuzzer coverage
compile_set fuzz $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link
# 2) same code without the fuzzer runtime hooks, for the standalone reproducers
compile_set standalone $SANITIZER_FLAGS $DEBUG_FLAGS -O1
# 3) normal-flags build for the oracle
compile_set normal -O1 $COVERAGE_FLAGS

"$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/lsan_off.o"
"$CC" $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/standalone_main.o"

SDL_LIBS=$(pkg-config --libs sdl3)
for h in "${HARNESSES[@]}"; do
  "$CXX" "${CXXSTD[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link "${INC[@]}" \
      -c "mayhem/$h.cpp" -o "$OUT/harness_$h.fuzz.o"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE \
      "$OUT/harness_$h.fuzz.o" $(lib_objs fuzz) "$OUT/lsan_off.o" $SDL_LIBS -o "$SRC/$h"

  "$CXX" "${CXXSTD[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS -O1 "${INC[@]}" \
      -c "mayhem/$h.cpp" -o "$OUT/harness_$h.standalone.o"
  "$CXX" $SANITIZER_FLAGS $DEBUG_FLAGS \
      "$OUT/harness_$h.standalone.o" "$OUT/standalone_main.o" $(lib_objs standalone) \
      "$OUT/lsan_off.o" $SDL_LIBS -o "$SRC/$h-standalone"
done

# KAT probe: normal flags, dynamically linked, no sanitizers.
"$CXX" "${CXXSTD[@]}" -O1 $COVERAGE_FLAGS "${INC[@]}" -c mayhem/kat_probe.cpp -o "$OUT/harness_kat_probe.normal.o"
"$CXX" $COVERAGE_FLAGS "$OUT/harness_kat_probe.normal.o" $(lib_objs normal) $SDL_LIBS -o "$SRC/kat_probe"
