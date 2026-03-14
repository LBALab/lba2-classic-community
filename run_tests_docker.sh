#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# run_tests_docker.sh — Build & run all LBA2 ASM-vs-CPP equivalence tests
#                       inside a Linux x86_64 Docker container.
#
# Usage:
#   ./run_tests_docker.sh              # Build & run all tests
#   ./run_tests_docker.sh --build-only # Build the Docker image without running
#   ./run_tests_docker.sh --rebuild    # Force rebuild the Docker image
#   ./run_tests_docker.sh test_getang2d test_lirot3df   # Run only named tests
# ──────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IMAGE_NAME="lba2-test"
CONTAINER_SRC="/src"
LOG_DIR="${SCRIPT_DIR}/build_logs"
mkdir -p "${LOG_DIR}"

PRESET="linux_test"
BUILD_ONLY=false
FORCE_REBUILD=false
RENDER_MODE=false
BISECT_MODE=false
TEST_NAMES=()

for arg in "$@"; do
    case "$arg" in
        --build-only)
            BUILD_ONLY=true
            ;;
        --rebuild)
            FORCE_REBUILD=true
            ;;
        --render)
            RENDER_MODE=true
            ;;
        --bisect)
            BISECT_MODE=true
            ;;
        *)
            TEST_NAMES+=("$arg")
            ;;
    esac
done

# ── Build Docker image (skip if already cached) ──────────────────────────────
IMAGE_EXISTS=$(docker images -q "${IMAGE_NAME}" 2>/dev/null)
if [ "$FORCE_REBUILD" = true ] || [ -z "$IMAGE_EXISTS" ]; then
    DOCKER_BUILD_LOG="${LOG_DIR}/docker_build_$(date +%Y%m%d_%H%M%S).log"
    echo "==> Building Docker image '${IMAGE_NAME}' (linux/amd64) …"
    echo "    Build log: ${DOCKER_BUILD_LOG}"
    if ! docker build --platform linux/amd64 \
            -t "${IMAGE_NAME}" \
            -f "${SCRIPT_DIR}/Dockerfile.test" \
            "${SCRIPT_DIR}" \
            2>&1 | tee "${DOCKER_BUILD_LOG}"; then
        echo "==> Docker build FAILED. See log: ${DOCKER_BUILD_LOG}"
        exit 1
    fi
    echo "==> Docker image built successfully."
else
    echo "==> Docker image '${IMAGE_NAME}' already exists (use --rebuild to force)."
fi

if [ "$BUILD_ONLY" = true ]; then
    echo "==> Exiting (--build-only)."
    exit 0
fi

# ── Build ctest filter from test names ────────────────────────────────────────
CTEST_REGEX=""
if [ ${#TEST_NAMES[@]} -gt 0 ]; then
    # Join test names with | for ctest -R regex
    CTEST_REGEX=$(IFS='|'; echo "${TEST_NAMES[*]}")
    echo "==> Running only: ${TEST_NAMES[*]}"
fi

# ── Run tests ─────────────────────────────────────────────────────────────────
TEST_LOG="${LOG_DIR}/test_run_$(date +%Y%m%d_%H%M%S).log"
echo "==> Running tests (preset: ${PRESET}) …"
# For render mode, we need read-write access to copy output files back
if [ "${RENDER_MODE}" = "true" ]; then
    MOUNT_OPTS="${SCRIPT_DIR}:${CONTAINER_SRC}"
else
    MOUNT_OPTS="${SCRIPT_DIR}:${CONTAINER_SRC}:ro"
fi

echo "    Test log: ${TEST_LOG}"
docker run --rm \
    --platform linux/amd64 \
    -v "${MOUNT_OPTS}" \
    -w "${CONTAINER_SRC}" \
    -e "CTEST_REGEX=${CTEST_REGEX}" \
    -e "CONTAINER_SRC=${CONTAINER_SRC}" \
    -e "PRESET=${PRESET}" \
    -e "RENDER_MODE=${RENDER_MODE}" \
    -e "BISECT_MODE=${BISECT_MODE}" \
    "${IMAGE_NAME}" \
    bash -c '
        set -e

        echo "--- Installing UASM ---"
        curl -fsSL https://github.com/Terraspace/UASM/releases/download/v2.57r/uasm257_linux64.zip \
            -o /tmp/uasm.zip
        unzip -oq /tmp/uasm.zip -d /usr/local/bin
        chmod +x /usr/local/bin/uasm
        uasm -? </dev/null 2>&1 | head -1 || true

        cp -a ${CONTAINER_SRC} /tmp/lba2
        cd /tmp/lba2

        echo "--- cmake configure (${PRESET}) ---"
        cmake -S . -B build \
            -DLBA2_BUILD_TESTS=ON \
            --preset ${PRESET}

        echo "--- cmake build ---"
        cmake --build build -j$(nproc)

        if [ "${RENDER_MODE}" = "true" ]; then
            echo "--- building polyrec replay programs ---"
            cmake --build build -j$(nproc) --target replay_polyrec_asm --target replay_polyrec_cpp
            echo "--- rendering polygon recordings ---"
            cd build/tests/SNAPSHOT
            for rec in /tmp/lba2/tests/SNAPSHOT/fixtures/*.lba2polyrec; do
                [ -f "$rec" ] || continue
                echo "Rendering: $(basename "$rec")"
                bash /tmp/lba2/tests/SNAPSHOT/render_polyrec.sh "$rec" \
                    /tmp/lba2/tests/SNAPSHOT/fixtures \
                    ./replay_polyrec_asm ./replay_polyrec_cpp || true
            done
            echo "--- copying rendered files to host ---"
            cp -f /tmp/lba2/tests/SNAPSHOT/fixtures/*.raw ${CONTAINER_SRC}/tests/SNAPSHOT/fixtures/ 2>/dev/null || true
            cp -f /tmp/lba2/tests/SNAPSHOT/fixtures/*.ppm ${CONTAINER_SRC}/tests/SNAPSHOT/fixtures/ 2>/dev/null || true
        elif [ "${BISECT_MODE}" = "true" ]; then
            echo "--- building polyrec replay programs ---"
            cmake --build build -j$(nproc) --target replay_polyrec_asm --target replay_polyrec_cpp
            echo "--- bisecting polygon calls ---"
            cd build/tests/SNAPSHOT
            for rec in /tmp/lba2/tests/SNAPSHOT/fixtures/*.lba2polyrec; do
                [ -f "$rec" ] || continue
                echo "Bisecting: $(basename "$rec")"
                bash /tmp/lba2/tests/SNAPSHOT/bisect_polyrec.sh "$rec" \
                    ./replay_polyrec_asm ./replay_polyrec_cpp
            done
        else
            echo "--- ctest ---"
            if [ -n "${CTEST_REGEX}" ]; then
                ctest --test-dir build --output-on-failure -R "${CTEST_REGEX}"
            else
                ctest --test-dir build --output-on-failure
            fi
        fi
    ' 2>&1 | tee "${TEST_LOG}"

echo "==> Test log saved: ${TEST_LOG}"
