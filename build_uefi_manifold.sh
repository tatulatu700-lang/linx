#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "============================================================"
echo " LINX UEFI MANIFOLD BUILD (PORTABLE COFF TARGET)"
echo "============================================================"
echo "[*] Repository : $SCRIPT_DIR"

# 1. Compile Assembly Entry Shim directly to COFF
echo "[*] Step 1: Compiling bootx64.s as PE-COFF object..."
clang \
    --target=x86_64-unknown-windows-msvc \
    -c bootx64.s \
    -o bootx64.obj

# 2. Compile Substrate Binary Container as PE-COFF
echo "[*] Step 2: Compiling substrate blob into PE-COFF object..."
cat << 'BLOB_EOF' > substrate_anchor.s
.section .rdata,"dr"
.globl _binary_linum_polyglot_bin_start
.globl _binary_linum_polyglot_bin_end
.balign 16
_binary_linum_polyglot_bin_start:
    .incbin "linum_polyglot.bin"
_binary_linum_polyglot_bin_end:
BLOB_EOF

clang \
    --target=x86_64-unknown-windows-msvc \
    -c substrate_anchor.s \
    -o substrate_blob.obj
rm -f substrate_anchor.s

# 3. Compile Freestanding UEFI Core
echo "[*] Step 3: Compiling linx_uefi_core.c as PE-COFF object..."
clang \
    --target=x86_64-unknown-windows-msvc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fshort-wchar \
    -mno-red-zone \
    -Wall \
    -Wextra \
    -Werror \
    -c linx_uefi_core.c \
    -o linx_uefi_core.obj

# 4. Link Pure UEFI PE32+ Executable via lld-link
echo "[*] Step 4: Linking native PE32+ UEFI executable (bootx64.efi)..."
lld-link \
    -subsystem:efi_application \
    -nodefaultlib \
    -entry:efi_main \
    bootx64.obj \
    linx_uefi_core.obj \
    substrate_blob.obj \
    -out:bootx64.efi

# 5. Clean up temporary COFF objects
rm -f bootx64.obj linx_uefi_core.obj substrate_blob.obj

# 6. Verify Artifact and Generate Seal
echo "[*] Step 5: Verifying EFI artifact integrity..."
test -f bootx64.efi
file bootx64.efi
sha256sum bootx64.efi > bootx64.efi.sha256
cat bootx64.efi.sha256

echo "[*] Step 6: Self-checking generated checksum..."
sha256sum -c bootx64.efi.sha256

echo
echo "[+] BARE-METAL MANIFOLD BUILT: EXIT 0"
