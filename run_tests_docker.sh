#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# run_tests_docker.sh — Build & run all LBA2 ASM-vs-CPP equivalence tests
#                       inside a Linux x86_64 Docker container.
#
# Usage:
#   ./run_tests_docker.sh              # Build & run tests
#   ./run_tests_docker.sh --build-only # Build the Docker image without running
#   ./run_tests_docker.sh --rebuild    # Force rebuild the Docker image
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

for arg in "$@"; do
    case "$arg" in
        --build-only)
            BUILD_ONLY=true
            ;;
        --rebuild)
            FORCE_REBUILD=true
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

# ── Run tests ─────────────────────────────────────────────────────────────────
TEST_LOG="${LOG_DIR}/test_run_$(date +%Y%m%d_%H%M%S).log"
echo "==> Running tests (preset: ${PRESET}) …"
echo "    Test log: ${TEST_LOG}"
docker run --rm \
    --platform linux/amd64 \
    -v "${SCRIPT_DIR}:${CONTAINER_SRC}:ro" \
    -w "${CONTAINER_SRC}" \
    "${IMAGE_NAME}" \
    bash -c "
        set -e

        # ── Install UASM at runtime (can't run x86_64 binary during build on ARM) ──
        echo '--- Installing UASM ---'
        curl -fsSL https://github.com/Terraspace/UASM/releases/download/v2.57r/uasm257_linux64.zip \
            -o /tmp/uasm.zip
        unzip -oq /tmp/uasm.zip -d /usr/local/bin
        chmod +x /usr/local/bin/uasm
        uasm -? </dev/null 2>&1 | head -1 || true

        # ── Copy source to writable tmpdir (mount is read-only) ──
        cp -a ${CONTAINER_SRC} /tmp/lba2
        cd /tmp/lba2

        echo '--- cmake configure (${PRESET}) ---'
        cmake -S . -B build \
            -DLBA2_BUILD_TESTS=ON \
            --preset ${PRESET}

        echo '--- cmake build ---'
        cmake --build build -j\$(nproc)

        echo '--- ctest ---'
        ctest --test-dir build --output-on-failure
    " 2>&1 | tee "${TEST_LOG}"

echo "==> Test log saved: ${TEST_LOG}"
