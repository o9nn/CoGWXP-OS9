#!/bin/bash
#
# CoGWXP-OS9 Master Build Script
#
# Build order (respecting dependencies):
# 1. cogutil (no dependencies)
# 2. plan9/9p (depends on cogutil)
# 3. inferno/dis (depends on cogutil)
# 4. atomspace (depends on cogutil, optionally 9p, dis)
# 5. pln (depends on atomspace)
# 6. cogserver (depends on pln)
# 7. cogw7os (depends on all above)
# 8. integration (depends on all above)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
INSTALL_DIR="${SCRIPT_DIR}/install"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Build configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
ENABLE_9P="${ENABLE_9P:-ON}"
ENABLE_DIS="${ENABLE_DIS:-ON}"
ENABLE_NT="${ENABLE_NT:-OFF}"
BUILD_TESTS="${BUILD_TESTS:-ON}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --no-9p)
            ENABLE_9P="OFF"
            shift
            ;;
        --no-dis)
            ENABLE_DIS="OFF"
            shift
            ;;
        --enable-nt)
            ENABLE_NT="ON"
            shift
            ;;
        --no-tests)
            BUILD_TESTS="OFF"
            shift
            ;;
        --clean)
            rm -rf "${BUILD_DIR}" "${INSTALL_DIR}"
            print_status "Cleaned build directories"
            exit 0
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug       Build with debug symbols"
            echo "  --release     Build optimized release (default)"
            echo "  --no-9p       Disable Plan 9 9P protocol integration"
            echo "  --no-dis      Disable Inferno Dis VM integration"
            echo "  --enable-nt   Enable Windows NT kernel integration"
            echo "  --no-tests    Skip building tests"
            echo "  --clean       Remove build directories"
            echo "  --help        Show this help message"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

print_header "CoGWXP-OS9 Build System"

echo "Configuration:"
echo "  Build type:     ${BUILD_TYPE}"
echo "  9P Integration: ${ENABLE_9P}"
echo "  Dis Integration: ${ENABLE_DIS}"
echo "  NT Integration: ${ENABLE_NT}"
echo "  Build tests:    ${BUILD_TESTS}"
echo "  Parallel jobs:  ${JOBS}"
echo ""

# Create build directories
mkdir -p "${BUILD_DIR}"
mkdir -p "${INSTALL_DIR}"

# ===========================================================================
# Build Plan 9 Components
# ===========================================================================

if [ "${ENABLE_9P}" = "ON" ]; then
    print_header "Building Plan 9 Components (9P Protocol)"
    
    P9_BUILD_DIR="${BUILD_DIR}/plan9"
    mkdir -p "${P9_BUILD_DIR}"
    
    cd "${P9_BUILD_DIR}"
    cmake "${SCRIPT_DIR}/plan9" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DBUILD_TESTS="${BUILD_TESTS}"
    
    make -j"${JOBS}"
    make install
    
    print_status "Plan 9 components built successfully"
fi

# ===========================================================================
# Build Inferno Components
# ===========================================================================

if [ "${ENABLE_DIS}" = "ON" ]; then
    print_header "Building Inferno Components (Dis VM)"
    
    DIS_BUILD_DIR="${BUILD_DIR}/inferno"
    mkdir -p "${DIS_BUILD_DIR}"
    
    cd "${DIS_BUILD_DIR}"
    cmake "${SCRIPT_DIR}/inferno" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
        -DBUILD_TESTS="${BUILD_TESTS}"
    
    make -j"${JOBS}"
    make install
    
    print_status "Inferno components built successfully"
fi

# ===========================================================================
# Build OpenCog Components
# ===========================================================================

print_header "Building OpenCog Components"

OPENCOG_BUILD_DIR="${BUILD_DIR}/opencog"
mkdir -p "${OPENCOG_BUILD_DIR}"

cd "${OPENCOG_BUILD_DIR}"
cmake "${SCRIPT_DIR}/opencog" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DENABLE_9P_INTEGRATION="${ENABLE_9P}" \
    -DENABLE_DIS_INTEGRATION="${ENABLE_DIS}" \
    -DENABLE_NT_INTEGRATION="${ENABLE_NT}" \
    -DBUILD_TESTS="${BUILD_TESTS}" \
    -DCMAKE_PREFIX_PATH="${INSTALL_DIR}"

make -j"${JOBS}"
make install

print_status "OpenCog components built successfully"

# ===========================================================================
# Build CogW7OS Components
# ===========================================================================

print_header "Building CogW7OS Cognitive Kernel"

COGW7OS_BUILD_DIR="${BUILD_DIR}/cogw7os"
mkdir -p "${COGW7OS_BUILD_DIR}"

cd "${COGW7OS_BUILD_DIR}"
cmake "${SCRIPT_DIR}/cogw7os" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DENABLE_9P_INTEGRATION="${ENABLE_9P}" \
    -DENABLE_DIS_INTEGRATION="${ENABLE_DIS}" \
    -DENABLE_NT_INTEGRATION="${ENABLE_NT}" \
    -DBUILD_TESTS="${BUILD_TESTS}" \
    -DCMAKE_PREFIX_PATH="${INSTALL_DIR}"

make -j"${JOBS}"
make install

print_status "CogW7OS components built successfully"

# ===========================================================================
# Build Integration Layer
# ===========================================================================

print_header "Building Integration Layer"

INTEGRATION_BUILD_DIR="${BUILD_DIR}/integration"
mkdir -p "${INTEGRATION_BUILD_DIR}"

cd "${INTEGRATION_BUILD_DIR}"
cmake "${SCRIPT_DIR}/integration" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
    -DENABLE_9P_INTEGRATION="${ENABLE_9P}" \
    -DENABLE_DIS_INTEGRATION="${ENABLE_DIS}" \
    -DENABLE_NT_INTEGRATION="${ENABLE_NT}" \
    -DBUILD_TESTS="${BUILD_TESTS}" \
    -DCMAKE_PREFIX_PATH="${INSTALL_DIR}"

make -j"${JOBS}"
make install

print_status "Integration layer built successfully"

# ===========================================================================
# Run Tests
# ===========================================================================

if [ "${BUILD_TESTS}" = "ON" ]; then
    print_header "Running Tests"
    
    cd "${BUILD_DIR}"
    ctest --output-on-failure || print_warning "Some tests failed"
    
    print_status "Tests completed"
fi

# ===========================================================================
# Summary
# ===========================================================================

print_header "Build Complete"

echo ""
echo "Installation directory: ${INSTALL_DIR}"
echo ""
echo "Libraries:"
ls -la "${INSTALL_DIR}/lib"/*.so* 2>/dev/null || ls -la "${INSTALL_DIR}/lib"/*.a 2>/dev/null || echo "  (none found)"
echo ""
echo "Headers:"
ls -la "${INSTALL_DIR}/include/cogwxp"/*.h 2>/dev/null || echo "  (none found)"
echo ""
echo "To use the libraries, set:"
echo "  export LD_LIBRARY_PATH=${INSTALL_DIR}/lib:\$LD_LIBRARY_PATH"
echo "  export PKG_CONFIG_PATH=${INSTALL_DIR}/lib/pkgconfig:\$PKG_CONFIG_PATH"
echo ""

print_status "CoGWXP-OS9 build completed successfully!"
