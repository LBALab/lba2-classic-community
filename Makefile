# Convenience targets — delegate to CMake and scripts (see docs/GAME_DATA.md).
.PHONY: help clean build run build-run test-discovery test tests test-docker format-check save-probe-lz-selftest

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(shell "$(MAKEFILE_DIR)scripts/dev/repo_root.sh" 2>/dev/null || echo "$(MAKEFILE_DIR)")
BUILD_DIR ?= $(REPO_ROOT)/build
CMAKE ?= cmake
NINJA ?= ninja

help:
	@echo "Targets:"
	@echo "  make clean          - remove build tree (\$$BUILD_DIR, default: build/)"
	@echo "  make build          - configure (Ninja) and build lba2"
	@echo "  make run            - build and run (uses scripts/dev/build-and-run.sh)"
	@echo "  make build-run      - same as run"
	@echo "  make test-discovery - configure with tests, build host tests (discovery + console + savegame), run CTest"
	@echo "  make test | tests   - same as test-discovery (host path tests; no retail files)"
	@echo "  make test-docker    - ./run_tests_docker.sh (ASM suite; requires Docker)"
	@echo "  make format-check   - scripts/ci/check-format.sh"
	@echo "  make save-probe-lz-selftest - build save_decompress + run LZ golden self-test"

clean:
	rm -rf "$(BUILD_DIR)"

build:
	$(CMAKE) -S "$(REPO_ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build "$(BUILD_DIR)" --target lba2

run build-run:
	@bash "$(REPO_ROOT)/scripts/dev/build-and-run.sh"

test-discovery:
	$(CMAKE) -S "$(REPO_ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
		-DLBA2_BUILD_TESTS=ON \
		-DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
	$(CMAKE) --build "$(BUILD_DIR)" --target test_res_discovery test_console_state test_console_commands test_savegame_load_bounds
	cd "$(BUILD_DIR)" && ctest -R 'test_(res_discovery|console_state|console_commands|savegame_load_bounds)' --output-on-failure

test tests: test-discovery

test-docker:
	@cd "$(REPO_ROOT)" && ./run_tests_docker.sh

format-check:
	@bash "$(REPO_ROOT)/scripts/ci/check-format.sh"

save-probe-lz-selftest:
	$(CMAKE) -S "$(REPO_ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
		-DLBA2_BUILD_SAVE_TOOLS=ON
	$(CMAKE) --build "$(BUILD_DIR)" --target save_decompress
	python3 "$(REPO_ROOT)/scripts/save_probe_lz_selftest.py"
