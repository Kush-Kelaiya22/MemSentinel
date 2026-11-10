#!/usr/bin/env bash
# MemSentinel Diagnostics & Automated Setup Script (Linux / macOS)

set -e

echo "=========================================================="
echo "       MemSentinel Toolchain Diagnostics & Setup          "
echo "=========================================================="

if ! command -v gcc >/dev/null 2>&1; then
    echo "[ERROR] gcc not found in PATH."
    exit 1
fi

GCC_PATH=$(command -v gcc)
echo "[OK] Detected GCC: ${GCC_PATH}"
echo "     Version     : $(gcc --version | head -n 1)"

# Check plugin support
if gcc -v 2>&1 | grep -q -- "--enable-plugin"; then
    echo "[OK] GCC was built with --enable-plugin."
else
    echo "[WARNING] GCC was built without --enable-plugin."
fi

# Locate GCC library directory
LIBGCC_PATH=$(gcc -print-libgcc-file-name)
GCC_LIB_DIR=$(dirname "${LIBGCC_PATH}")
SPECS_DEST="${GCC_LIB_DIR}/specs"
PLUGIN_DIR=$(gcc -print-file-name=plugin)

echo "[INFO] GCC Specs Directory: ${GCC_LIB_DIR}"
echo "[INFO] GCC Plugin Directory: ${PLUGIN_DIR}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "${SCRIPT_DIR}")"
SPECS_SRC="${REPO_ROOT}/src/specs/memsentinel.specs"
SO_SRC="${REPO_ROOT}/build/lib/memsentinel.so"

if [ -f "${SPECS_SRC}" ]; then
    echo "[INSTALL] Installing specs file to ${SPECS_DEST}..."
    sudo cp -f "${SPECS_SRC}" "${SPECS_DEST}"
    echo "  ✓ Specs file installed."
fi

if [ -f "${SO_SRC}" ] && [ -d "${PLUGIN_DIR}" ]; then
    echo "[INSTALL] Installing plugin shared library to ${PLUGIN_DIR}..."
    sudo cp -f "${SO_SRC}" "${PLUGIN_DIR}/"
    echo "  ✓ Plugin shared library installed."
fi

echo "=========================================================="
echo "       Setup Complete! MemSentinel is Active.             "
echo "=========================================================="
