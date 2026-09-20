#!/usr/bin/env bash
set -euo pipefail

WORKSPACE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="$WORKSPACE/../out"
VENV="${VENV:-$WORKSPACE/../edk2-venv}"

mkdir -p "$OUT_DIR"

echo "=== Step 0: Ensuring Submodule Patches & Assets ==="
"$WORKSPACE/scripts/apply_submodule_patches.sh"

export PATH="$VENV/bin:$WORKSPACE/MU_BASECORE/BaseTools/Source/C/bin:$WORKSPACE/MU_BASECORE/BaseTools/BinWrappers/PosixLike:$PATH"
export EDK_TOOLS_PATH="$WORKSPACE/MU_BASECORE/BaseTools"
export BASE_TOOLS_PATH="$WORKSPACE/MU_BASECORE/BaseTools"
export CONF_PATH="$WORKSPACE/Conf"
export PYTHON_COMMAND="$VENV/bin/python3"
export CCACHE_DIR="$WORKSPACE/.ccache"
export CCACHE_MAXSIZE="0"

# Compiler toolchain
export CLANG_BIN="/usr/bin/"
export CC="ccache clang"
export CXX="ccache clang++"
export GCC5_AARCH64_PREFIX="aarch64-linux-gnu-"

cd "$WORKSPACE"

echo "=== Step 1: Building BootShim for SM8650 (Base: 0xF3800000, Size: 0x00400000) ==="
./build_boot_shim.sh -b 0xF3800000 -s 0x00400000

echo "=== Step 2: Building UEFI Firmware for TB520FU (LanaiPkg / qcom-8650) ==="
"$PYTHON_COMMAND" Platforms/LanaiPkg/PlatformBuildNoSb.py \
    TARGET=RELEASE \
    TARGET_DEVICE=qcom-8650 \
    TOOL_CHAIN_TAG=CLANGPDB

echo "=== Step 3: Verifying and Packaging Output ==="
if [ -f "Build/LanaiPkg/qcom-8650.img" ]; then
    cp "Build/LanaiPkg/qcom-8650.img" "$OUT_DIR/tb520fu-edk2-uefi-boot.img"
    echo "=== SUCCESS: Output generated at $OUT_DIR/tb520fu-edk2-uefi-boot.img ==="
    sha256sum "$OUT_DIR/tb520fu-edk2-uefi-boot.img"
fi
