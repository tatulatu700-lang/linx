# LINX

## Bare-Metal Neuro-Symbolic Invariant Eprom

LINX is a low-level x86_64 execution substrate targeting UEFI firmware and
direct hardware framebuffer scanout.

The current implementation packages a freestanding execution vector into a
UEFI PE/COFF application. The UEFI entry point receives the firmware system
table, locates the Graphics Output Protocol (GOP), obtains the framebuffer
address, binds that address into the substrate context, and dispatches into
the embedded `linum_polyglot.bin` payload.

The project is intentionally low-level. It is not a conventional Linux
application and does not depend on a host dynamic linker or libc for the UEFI
execution path.

---

## Architecture

```text
                         UEFI FIRMWARE
                               |
                               v
                          bootx64.s
                               |
                               v
                efi_main(ImageHandle, SystemTable)
                               |
                               v
                       linx_uefi_core
                               |
                               v
                     UEFI Boot Services
                               |
                         LocateProtocol
                               |
                               v
                         GOP interface
                               |
                               v
                       FrameBufferBase
                               |
                               v
                     SubstrateContext
                               |
                               v
                    linum_polyglot.bin
                         + 0x3E0
                               |
                               v
                       execution vector
                               |
                               v
                    invariant validation
                               |
                               v
                         GOP scanout
```

---

## Core Components

### `bootx64.s`

Minimal x86_64 UEFI entry shim.

The entry point receives:

```text
RCX = UEFI image handle
RDX = UEFI system table
```

The shim establishes the required x86_64 calling convention and transfers
execution to the LINX UEFI dispatcher.

### `linx_uefi_core.c`

Freestanding UEFI core.

The implementation:

- accesses UEFI Boot Services;
- locates the Graphics Output Protocol;
- obtains the framebuffer base address;
- constructs the aligned substrate context;
- dispatches the embedded LINX vector;
- validates the returned execution invariants; and
- preserves the GOP display state during execution.

### `linum_polyglot.bin`

Embedded execution substrate.

The UEFI dispatcher resolves the vector entry at:

```text
linum_polyglot.bin + 0x3E0
```

The payload is linked into the UEFI image as a raw binary object.

### `build_uefi_manifold.sh`

Canonical UEFI build and verification pipeline.

The build performs:

1. binary-to-ELF object conversion;
2. freestanding compilation;
3. x86_64 assembly;
4. PE/COFF UEFI linking;
5. EFI artifact generation; and
6. SHA-256 integrity generation.

---

## Build Environment

Required tools:

```text
clang
GNU assembler
llvm lld-link
GNU objcopy
sha256sum
```

Target:

```text
Architecture : x86_64
Firmware     : UEFI
Binary       : PE32+ EFI application
Runtime      : freestanding / zero-libc
Graphics     : UEFI GOP
```

---

## Build

From the repository root:

```bash
chmod +x ./build_uefi_manifold.sh
./build_uefi_manifold.sh
```

Primary artifact:

```text
bootx64.efi
```

Integrity record:

```text
bootx64.efi.sha256
```

---

## Verification

Verify the EFI artifact:

```bash
sha256sum -c bootx64.efi.sha256
```

Verify the embedded substrate:

```bash
sha256sum linum_polyglot.bin
```

If present, verify the complete substrate manifest:

```bash
sha256sum -c SUBSTRATE_MANIFEST.sha256
```

The verification model deliberately separates source state, build state, EFI
artifact, SHA-256 integrity, and UEFI execution. A successful local build is
not equivalent to successful firmware execution.
