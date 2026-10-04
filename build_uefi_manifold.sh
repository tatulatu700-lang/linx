#!/bin/bash
set -euo pipefail
cd /home/ron/linx

echo "[*] Step 1: Writing PE-COFF compatible assembly entry shim..."
cat << 'ASM_EOF' > bootx64.s
.intel_syntax noprefix
.text
.globl efi_main

efi_main:
    # MS x64 ABI: RCX = ImageHandle, RDX = SystemTable
    sub rsp, 40
    and rsp, -16
    call linx_uefi_dispatch
    add rsp, 40
    ret
ASM_EOF

echo "[*] Step 2: Writing embedded substrate linker anchor..."
cat << 'SUB_EOF' > substrate_blob.s
.intel_syntax noprefix
.section .rdata,"dr"
.globl _binary_linum_polyglot_bin_start
.balign 16

_binary_linum_polyglot_bin_start:
    .incbin "linum_polyglot.bin"
SUB_EOF

echo "[*] Step 3: Compiling PE-COFF objects with Clang..."
clang -target x86_64-unknown-windows \
      -ffreestanding \
      -fno-stack-protector \
      -fno-stack-check \
      -mno-red-zone \
      -c bootx64.s -o bootx64.obj

clang -target x86_64-unknown-windows \
      -ffreestanding \
      -fno-stack-protector \
      -fno-stack-check \
      -mno-red-zone \
      -c substrate_blob.s -o substrate_blob.obj

clang -target x86_64-unknown-windows \
      -O2 \
      -ffreestanding \
      -fno-stack-protector \
      -fno-stack-check \
      -fshort-wchar \
      -mno-red-zone \
      -Wall -Wextra -Werror \
      -c linx_uefi_core.c -o linx_uefi_core.obj

echo "[*] Step 4: Linking native PE32+ UEFI executable (bootx64.efi)..."
lld-link -subsystem:efi_application \
         -nodefaultlib \
         -entry:efi_main \
         bootx64.obj \
         substrate_blob.obj \
         linx_uefi_core.obj \
         -out:bootx64.efi

echo "[*] Step 5: Verifying executable format and cryptographic seal..."
file bootx64.efi
sha256sum bootx64.efi > bootx64.efi.sha256
cat bootx64.efi.sha256

echo ""
echo "[+] BARE-METAL MANIFOLD BUILT: EXIT 0"
