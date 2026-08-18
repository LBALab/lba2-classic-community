# Convenience targets — delegate to CMake and scripts (see docs/GAME_DATA.md).
.PHONY: help clean build run build-run test tests test-docker format-check arch-check docs-links docs-symbols action-shell-check action-shell-selftest build-graph-check build-graph-selftest automation-index check-tooling save-probe-lz-selftest savegame-corpus

MAKEFILE_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(shell "$(MAKEFILE_DIR)scripts/dev/repo_root.sh" 2>/dev/null || echo "$(MAKEFILE_DIR)")
BUILD_DIR ?= $(REPO_ROOT)/build
CMAKE ?= cmake
NINJA ?= ninja
GAME_DIR ?=
TIMEOUT ?=
ABIS ?=

help:
	@echo "Targets:"
	@echo "  make clean          - remove build tree (\$$BUILD_DIR, default: build/)"
	@echo "  make build          - configure (Ninja) and build lba2"
	@echo "  make run            - build and run (uses scripts/dev/build-and-run.sh)"
	@echo "  make build-run      - same as run"
	@echo "  make test | tests   - configure with tests, build host_tests, run CTest -L host_quick (no Docker, no retail files)"
	@echo "  make test-docker    - ./run_tests_docker.sh (ASM suite; requires Docker)"
	@echo "  make format-check   - scripts/ci/check-format.sh"
	@echo "  make arch-check     - scripts/ci/check-arch.py (the boundaries CODESTYLE and AGENTS state)"
	@echo "  make docs-links     - scripts/ci/check-docs-links.sh (needs lychee for the link half)"
	@echo "  make docs-symbols   - scripts/ci/check-docs-symbols.py (a doc names a symbol; is it in that file?)"
	@echo "  make action-shell-check - scripts/ci/check-action-shell.py (shellcheck the run: blocks in composite actions)"
	@echo "  make action-shell-selftest - self-test for the above"
	@echo "  make build-graph-check - scripts/ci/check-build-graph.py (rules that need a built tree)"
	@echo "  make build-graph-selftest - self-test for the above"
	@echo "  make automation-index - regenerate tests/automation/README.md from the fixtures"
	@echo "  make check-tooling  - report which external tools this clone has (docs/TOOLING.md)"
	@echo "  make save-probe-lz-selftest - build save_decompress + run LZ golden self-test"
	@echo "  make savegame-corpus - run bundled save corpus harness (retail game data required)"

clean:
	rm -rf "$(BUILD_DIR)"

build:
	$(CMAKE) -S "$(REPO_ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug
	$(CMAKE) --build "$(BUILD_DIR)"

run build-run:
	@bash "$(REPO_ROOT)/scripts/dev/build-and-run.sh"

test tests:
	$(CMAKE) -S "$(REPO_ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
		-DLBA2_BUILD_TESTS=ON \
		-DLBA2_BUILD_ASM_EQUIV_TESTS=OFF
	$(CMAKE) --build "$(BUILD_DIR)" --target host_tests
	cd "$(BUILD_DIR)" && ctest -L host_quick --output-on-failure

test-docker:
	@cd "$(REPO_ROOT)" && ./run_tests_docker.sh

format-check:
	@bash "$(REPO_ROOT)/scripts/ci/check-format.sh"

arch-check:
	@python3 "$(REPO_ROOT)/scripts/ci/check-arch.py"

docs-links:
	@bash "$(REPO_ROOT)/scripts/ci/check-docs-links.sh"

docs-symbols:
	@python3 "$(REPO_ROOT)/scripts/ci/check-docs-symbols.py"

action-shell-check:
	@python3 "$(REPO_ROOT)/scripts/ci/check-action-shell.py"

action-shell-selftest:
	@python3 "$(REPO_ROOT)/scripts/ci/check-action-shell-selftest.py"

build-graph-check:
	@python3 "$(REPO_ROOT)/scripts/ci/check-build-graph.py"

build-graph-selftest:
	@python3 "$(REPO_ROOT)/scripts/ci/check-build-graph-selftest.py"

automation-index:
	@python3 "$(REPO_ROOT)/scripts/ci/gen-automation-index.py"

check-tooling:
	@bash "$(REPO_ROOT)/scripts/dev/check-tooling.sh"

save-probe-lz-selftest:
	$(CMAKE) -S "$(REPO_ROOT)" -B "$(BUILD_DIR)" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
		-DLBA2_BUILD_SAVE_TOOLS=ON
	$(CMAKE) --build "$(BUILD_DIR)" --target save_decompress
	python3 "$(REPO_ROOT)/scripts/save_probe_lz_selftest.py"

savegame-corpus:
	@bash "$(REPO_ROOT)/scripts/dev/run-savegame-corpus.sh" \
		$(if $(GAME_DIR),--game-dir "$(GAME_DIR)",) \
		$(if $(TIMEOUT),--timeout "$(TIMEOUT)",) \
		$(if $(ABIS),--abis "$(ABIS)",)
