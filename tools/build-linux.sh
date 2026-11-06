#!/usr/bin/env bash
# Builds WaterAA.dll on Linux with clang-cl (cmake/toolchain-linux-clang-cl.cmake)
# and runs the tests: the pure ones built for the host, the device ones and
# the DLL's own under Wine.
#
#   tools/build-linux.sh              build and test
#   tools/build-linux.sh --deploy     then copy WaterAA.dll and WaterAA.ini into
#                                     the game's Data/OBSE/Plugins (the INI only
#                                     where none stands yet)
#   tools/build-linux.sh --no-wine    skip the tests that need Wine
#
# WATERAA_GAME_DIRS: the game folders to deploy into, separated by colons;
# default Steam's Oblivion on Linux and the Windows installation mounted at
# /run/media/$USER/Programs.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$root/build-linux-clangcl"
host="$root/build-host"
deploy=0
wine_tests=1
for arg in "$@"; do
	case "$arg" in
		--deploy) deploy=1 ;;
		--no-wine) wine_tests=0 ;;
		-h|--help) sed -n '2,15p' "${BASH_SOURCE[0]}"; exit 0 ;;
		*) echo "unknown option: $arg" >&2; exit 2 ;;
	esac
done

mkdir -p "$host"
for test in WaterShaders ShaderClasses PluginEntry; do
	sources=("$root/tests/${test}Test.cpp")
	[[ $test == WaterShaders ]] && sources+=("$root/src/WaterShaders.cpp")
	[[ $test == PluginEntry ]] && sources+=("$root/src/PluginEntry.cpp")
	clang++ -std=c++17 -Wall -Wextra -Werror -I"$root/src" -I"$root/tests" "${sources[@]}" -o "$host/${test}Test"
	"$host/${test}Test" | grep -E "  FAIL  |all passed|FAILED" | sed "s/^/${test}Test: /"
done

if [[ ! -f "$build/build.ninja" ]]; then
	cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		--toolchain "$root/cmake/toolchain-linux-clang-cl.cmake"
fi
cmake --build "$build"

if [[ $wine_tests -eq 1 ]]; then
	for test in DeviceHookTest SetupTest; do
		(cd "$build" && WINEDEBUG=-all wine "./$test.exe" | grep -E "  FAIL  |all passed|FAILED|^device:" | sed "s/^/$test: /")
	done
	(cd "$build" && WINEDEBUG=-all wine ./ExportsTest.exe WaterAA.dll | grep -E "FAIL|all passed|failed" | sed "s/^/ExportsTest: /")
fi

if [[ $deploy -eq 1 ]]; then
	IFS=: read -r -a games <<< "${WATERAA_GAME_DIRS:-$HOME/.local/share/Steam/steamapps/common/Oblivion:/run/media/$USER/Programs/Steam/steamapps/common/Oblivion}"
	for game in "${games[@]}"; do
		plugins="$game/Data/OBSE/Plugins"
		if [[ ! -d "$plugins" ]]; then
			echo "No $plugins - not deployed there"
			continue
		fi
		cp "$build/WaterAA.dll" "$plugins/.WaterAA.dll.new"
		mv -f "$plugins/.WaterAA.dll.new" "$plugins/WaterAA.dll"
		[[ -f "$plugins/WaterAA.ini" ]] || cp "$root/WaterAA.ini" "$plugins/WaterAA.ini"
		echo "Deployed WaterAA.dll $(md5sum "$build/WaterAA.dll" | cut -c1-8) to $plugins"
	done
fi
