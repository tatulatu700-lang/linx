#include <stdint.h>

typedef void *EFI_HANDLE;
typedef uint64_t EFI_STATUS;
typedef uint64_t UINTN;

#define EFI_SUCCESS 0

typedef struct {
    uint64_t reserved;     /* +0x00: Buffer Size (0x4000) */
    uint64_t stamp;        /* +0x08: Out Stamp (0x4D554E494C5F3252 -> 'R2_LINUM') */
    uint64_t input_ptr;    /* +0x10: Input Matrix Vector Pointer */
    uint64_t canvas_ptr;   /* +0x18: Output Framebuffer Destination */
    uint64_t stride;       /* +0x20: Framebuffer Scanline Stride */
    uint64_t ticks;        /* +0x28: Execution Cycle Counter */
    uint64_t h1_state;     /* +0x30: Activation Vector Hash */
    uint64_t h2_state;     /* +0x38: Secondary Activation Hash */
} SubstrateContext;

extern uint8_t _binary_linum_polyglot_bin_start[];
extern uint8_t _binary_linum_polyglot_bin_end[];

typedef struct {
    uint32_t MaxMode;
    uint32_t Mode;
    void *Info;
    uint64_t SizeOfInfo;
    uint64_t FrameBufferBase;
    uint64_t FrameBufferSize;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    void *QueryMode;
    void *SetMode;
    void *Blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef struct {
    uint8_t Data[16];
} EFI_GUID;

static EFI_GUID gop_guid = {
    { 0xde, 0xa9, 0x42, 0x90, 0xdc, 0x23, 0xd4, 0x11,
      0x9e, 0x97, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d }
};

typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void *Reset;
    EFI_STATUS (__attribute__((ms_abi)) *OutputString)(struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This, const uint16_t *String);
    void *TestString;
    void *QueryMode;
    void *SetMode;
    void *SetAttribute;
    void *ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
} EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef struct {
    uint32_t Type;
    uint32_t Pad;
    uint64_t PhysicalStart;
    uint64_t VirtualStart;
    uint64_t NumberOfPages;
    uint64_t Attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef struct _EFI_BOOT_SERVICES {
    char Header[24];
    void *RaiseTPL; void *RestoreTPL;
    EFI_STATUS (__attribute__((ms_abi)) *AllocatePages)(uint32_t Type, uint32_t MemoryType, UINTN Pages, uint64_t *Memory);
    void *FreePages;
    EFI_STATUS (__attribute__((ms_abi)) *GetMemoryMap)(
        UINTN *MemoryMapSize,
        EFI_MEMORY_DESCRIPTOR *MemoryMap,
        UINTN *MapKey,
        UINTN *DescriptorSize,
        uint32_t *DescriptorVersion
    );
    EFI_STATUS (__attribute__((ms_abi)) *AllocatePool)(uint32_t PoolType, UINTN Size, void **Buffer);
    EFI_STATUS (__attribute__((ms_abi)) *FreePool)(void *Buffer);
    void *CreateEvent; void *SetTimer; void *WaitForEvent;
    void *SignalEvent; void *CloseEvent; void *CheckEvent;
    void *InstallProtocolInterface; void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface; void *HandleProtocol;
    void *Void; void *RegisterProtocolNotify;
    void *LocateHandle; void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage; void *StartImage; void *Exit;
    void *UnloadImage;
    EFI_STATUS (__attribute__((ms_abi)) *ExitBootServices)(EFI_HANDLE ImageHandle, UINTN MapKey);
    void *GetNextMonotonicCount;
    EFI_STATUS (__attribute__((ms_abi)) *Stall)(uint64_t Microseconds);
    EFI_STATUS (__attribute__((ms_abi)) *SetWatchdogTimer)(uint64_t Timeout, uint64_t WatchdogCode, uint64_t DataSize, uint16_t *WatchdogData);
    void *ConnectController; void *DisconnectController;
    void *OpenProtocol; void *CloseProtocol; void *OpenProtocolInformation;
    void *ProtocolsPerHandle; void *LocateHandleBuffer;
    EFI_STATUS (__attribute__((ms_abi)) *LocateProtocol)(EFI_GUID *Protocol, void *Registration, void **Interface);
} EFI_BOOT_SERVICES;

typedef struct {
    char Header[24];
    uint16_t *FirmwareVendor;
    uint32_t FirmwareRevision;
    void *ConsoleInHandle; void *ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;
    void *StandardErrorHandle; void *StdErr;
    void *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
} EFI_SYSTEM_TABLE;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void uart_puts(const char *str) {
    while (*str) {
        outb(0x3F8, (uint8_t)*str++);
    }
}

/* Enable hardware AVX state saving in CR4 and XCR0 */
static void enable_avx(void) {
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1ULL << 9);   /* OSFXSR: OS support for FXSAVE/FXRSTOR */
    cr4 |= (1ULL << 10);  /* OSXMMEXCPT: OS support for unmasked SIMD floating-point exceptions */
    cr4 |= (1ULL << 18);  /* OSXSAVE: XSAVE and Processor Extended States */
    __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));

    /* Enable x87 (bit 0), SSE (bit 1), and AVX (bit 2) states in XCR0 */
    uint32_t eax = 0x7;
    uint32_t edx = 0;
    uint32_t ecx = 0;
    __asm__ volatile("xsetbv" : : "a"(eax), "d"(edx), "c"(ecx));
}

EFI_STATUS __attribute__((ms_abi)) efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    if (!SystemTable || !SystemTable->BootServices) {
        for (;;) { __asm__ volatile("cli; hlt"); }
    }

    EFI_BOOT_SERVICES *bs = SystemTable->BootServices;

    if (bs->SetWatchdogTimer) {
        bs->SetWatchdogTimer(0, 0, 0, 0);
    }

    if (SystemTable->ConOut && SystemTable->ConOut->OutputString) {
        static const uint16_t msg[] = {
            '[', '+', ']', ' ', 'L', 'I', 'N', 'X', ':', ' ', 'P', 'o', 'l', 'y', 'g', 'l', 'o', 't', ' ',
            'S', 'u', 'b', 's', 't', 'r', 'a', 't', 'e', ' ', 'D', 'i', 's', 'p', 'a', 't', 'c', 'h', '\r', '\n', 0
        };
        SystemTable->ConOut->OutputString(SystemTable->ConOut, msg);
    }

    /* Locate GOP Framebuffer */
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS status = bs->LocateProtocol(&gop_guid, 0, (void **)&gop);
    uint32_t *fb = 0;
    uint64_t total_pixels = 0;

    if (status == EFI_SUCCESS && gop && gop->Mode && gop->Mode->FrameBufferBase) {
        fb = (uint32_t *)gop->Mode->FrameBufferBase;
        total_pixels = gop->Mode->FrameBufferSize / 4;
        for (uint64_t i = 0; i < total_pixels; i++) {
            fb[i] = 0x000F172A; /* Deep slate baseline */
        }
    }

    /* Allocate an explicit executable buffer for polyglot substrate (EfiLoaderCode = 1) */
    uint64_t poly_size = (uint64_t)(_binary_linum_polyglot_bin_end - _binary_linum_polyglot_bin_start);
    UINTN pages = (poly_size + 4095) / 4096;
    uint64_t exec_arena = 0;
    status = bs->AllocatePages(0 /* AllocateAnyPages */, 1 /* EfiLoaderCode */, pages, &exec_arena);
    if (status == EFI_SUCCESS && exec_arena) {
        uint8_t *dst = (uint8_t *)exec_arena;
        const uint8_t *src = _binary_linum_polyglot_bin_start;
        for (uint64_t i = 0; i < poly_size; i++) {
            dst[i] = src[i];
        }
    } else {
        exec_arena = (uint64_t)_binary_linum_polyglot_bin_start;
    }

    /* Exit Boot Services */
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *mem_map = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_ver = 0;

    bs->GetMemoryMap(&map_size, mem_map, &map_key, &desc_size, &desc_ver);
    map_size += 4096;
    bs->AllocatePool(2 /* EfiBootServicesData */, map_size, (void **)&mem_map);
    bs->GetMemoryMap(&map_size, mem_map, &map_key, &desc_size, &desc_ver);

    status = bs->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        map_size += 4096;
        bs->GetMemoryMap(&map_size, mem_map, &map_key, &desc_size, &desc_ver);
        bs->ExitBootServices(ImageHandle, map_key);
    }

    /* ==================================================================== */
    /* RING 0 / LONG-MODE BARE METAL VECTOR EXECUTION                       */
    /* ==================================================================== */

    /* 1. Arm CPU for AVX / AVX2 execution */
    enable_avx();
    uart_puts("[BARE-METAL] AVX2 & OSXSAVE enabled in hardware CR4/XCR0.\r\n");

    /* 2. Configure Substrate Context Wire Invariant */
    SubstrateContext ctx;
    ctx.reserved   = 0x4000;
    ctx.stamp      = 0;
    ctx.input_ptr  = 0;
    ctx.canvas_ptr = (uint64_t)fb;
    ctx.stride     = 1024;
    ctx.ticks      = 0;
    ctx.h1_state   = 0;
    ctx.h2_state   = 0;

    const uint8_t *poly = (const uint8_t *)exec_arena;
    uint16_t mz_magic = *(const uint16_t *)(poly + 0x00);
    uint32_t elf_magic = *(const uint32_t *)(poly + 0x40);

    if (mz_magic == 0x5A4D && elf_magic == 0x464C457F) {
        uart_puts("[BARE-METAL] Polyglot container verified (MZ+ELF).\r\n");

        typedef uint64_t (*SubstrateVectorEntry)(SubstrateContext *ctx, uint64_t size);
        SubstrateVectorEntry dispatch = (SubstrateVectorEntry)(poly + 0x3E0);

        uart_puts("[BARE-METAL] Dispatching forward-pass vector at +0x3E0...\r\n");
        uint64_t ret_rax = dispatch(&ctx, 0x4000);

        if (ret_rax == 0x4C494E55 /* 'LINU' */ || ctx.stamp == 0x4D554E494C5F3252 /* 'R2_LINUM' */) {
            uart_puts("[BARE-METAL] Forward pass sound. Stamp: R2_LINUM verified.\r\n");
            if (fb) {
                for (uint64_t i = 0; i < (total_pixels > 102400 ? 102400 : total_pixels); i++) {
                    fb[i] = 0x0000FF4C; /* Linum verified canvas mark */
                }
            }
        }
    }

    /* Pinned halt state */
    for (;;) {
        __asm__ volatile("cli; hlt");
    }

    return EFI_SUCCESS;
}
