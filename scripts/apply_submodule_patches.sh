#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PATCH_DIR="$ROOT_DIR/patches"

echo "=== Ensuring Submodule Patches and Assets for SM8650 ==="

# 1. SurfaceDuoACPI 8650 assets
if [ -d "$PATCH_DIR/SurfaceDuoACPI_8650" ]; then
    mkdir -p "$ROOT_DIR/Platforms/SurfaceDuoACPI/8650"
    cp -r "$PATCH_DIR/SurfaceDuoACPI_8650/"* "$ROOT_DIR/Platforms/SurfaceDuoACPI/8650/"
    echo "[✓] SurfaceDuoACPI/8650 ACPI assets verified."
fi

# 2. Submodule patches
apply_patch_if_needed() {
    local target_dir="$1"
    local patch_file="$2"
    local name="$3"

    if [ -f "$patch_file" ] && [ -s "$patch_file" ]; then
        if git -C "$target_dir" apply --check "$patch_file" >/dev/null 2>&1; then
            git -C "$target_dir" apply "$patch_file"
            echo "[✓] Applied patch to $name."
        else
            echo "[-] Patch for $name already applied or not applicable."
        fi
    fi
}

apply_patch_if_needed "$ROOT_DIR/MU_BASECORE" "$PATCH_DIR/submodules/MU_BASECORE.patch" "MU_BASECORE"
apply_patch_if_needed "$ROOT_DIR/Common/MU" "$PATCH_DIR/submodules/Common_MU.patch" "Common/MU"
apply_patch_if_needed "$ROOT_DIR/Common/MU_TIANO" "$PATCH_DIR/submodules/Common_MU_TIANO.patch" "Common/MU_TIANO"
apply_patch_if_needed "$ROOT_DIR/Silicon/Arm/MU_TIANO" "$PATCH_DIR/submodules/Silicon_Arm_MU_TIANO.patch" "Silicon/Arm/MU_TIANO"
apply_patch_if_needed "$ROOT_DIR/Platforms/OpensslPkg/Library/OpensslLib/openssl" "$PATCH_DIR/submodules/OpensslPkg_openssl.patch" "OpensslLib/openssl"

echo "=== All Submodule Patches Verified ==="
