#!/bin/sh
# Run the checks from CONSTRAINTS.md.
# Usage: tools/check.sh fast|task [BASE]
#   fast  build and ctest, for each edit
#   task  fast, then the floor guard, coverage, and clang-tidy, before each commit
set -eu

mode=${1:-task}
base=${2:-origin/main}
cd "$(dirname "$0")/.."

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build build -j "$(nproc)"
ctest --test-dir build --output-on-failure
[ "$mode" = fast ] && exit 0

python3 tools/floor_guard.py --base "$base"

cmake -S . -B build-cov -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS=--coverage > /dev/null
cmake --build build-cov -j "$(nproc)"
find build-cov -name '*.gcda' -delete
ctest --test-dir build-cov > /dev/null
python3 tools/coverage_diff.py build-cov --base "$base" --min 80

clang-tidy --quiet -p build src/*.cpp spike/*.cpp tests/*.cpp
echo "check: all $mode checks passed"
