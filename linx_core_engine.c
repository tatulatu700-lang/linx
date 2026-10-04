/*
 * LINX Core Engine - Zero-Copy Affine Weight Streaming & Activation Manifold (ZCA-WSAM)
 * Target: Intel Core Ultra 5 238V (Lunar Lake-M) x86_64
 * Workspace: /home/ron/linx
 */

#define SYS_read        0
#define SYS_write       1
#define SYS_open        2
#define SYS_close       3
#define SYS_mmap        9
#define SYS_mprotect   10
#define SYS_munmap     11
#define SYS_exit       60

#define O_RDONLY       00
#define PROT_READ     0x1
#define PROT_WRITE    0x2
#define PROT_EXEC     0x4
#define MAP_PRIVATE   0x02
#define MAP_ANONYMOUS 0x20

#define SUBSTRATE_SIZE 4096

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned char      u8;
typedef long long          s64;

typedef struct {
    u64 reserved;       /* +0x00: 0x4000 */
    u64 stamp;          /* +0x08: 0x4D554E494C5F3252 ('R2_LINUM') */
    u64 gguf_addr;      /* +0x10: 0x400200 */
    u64 fb_ptr;         /* +0x18: Linear Framebuffer scanout pointer */
    u32 gemv_checksum;  /* +0x20: Output reduction scalar */
    u32 _pad;           /* +0x24: Alignment padding */
    u64 custom_x_ptr;   /* +0x28: Optional custom input vector pointer (0 = default 0x220) */
    u64 hidden_h;       /* +0x30: Captured hidden layer activations */
} __attribute__((packed, aligned(16))) SubstrateContext;

struct GGUFHeader {
    u32 magic;              /* 0x46554747 ('GGUF') */
    u32 version;
    u64 tensor_count;
    u64 metadata_kv_count;
} __attribute__((packed));

enum GGUFType {
    GGUF_TYPE_UINT8   = 0,
    GGUF_TYPE_INT8    = 1,
    GGUF_TYPE_UINT16  = 2,
    GGUF_TYPE_INT16   = 3,
    GGUF_TYPE_UINT32  = 4,
    GGUF_TYPE_INT32   = 5,
    GGUF_TYPE_FLOAT32 = 6,
    GGUF_TYPE_BOOL    = 7,
    GGUF_TYPE_STRING  = 8,
    GGUF_TYPE_ARRAY   = 9,
    GGUF_TYPE_UINT64  = 10,
    GGUF_TYPE_INT64   = 11,
    GGUF_TYPE_FLOAT64 = 12
};

static inline u64 sys_call3(u64 nr, u64 a1, u64 a2, u64 a3) {
    u64 ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline u64 sys_call6(u64 nr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6) {
    u64 ret;
    register u64 r10 __asm__("r10") = a4;
    register u64 r8  __asm__("r8")  = a5;
    register u64 r9  __asm__("r9")  = a6;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline u64 read_tsc(void) {
    u32 lo, hi;
    __asm__ volatile ("lfence; rdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return ((u64)hi << 32) | lo;
}

static void print_msg(const char *msg, u64 len) {
    sys_call3(SYS_write, 1, (u64)msg, len);
}

static void print_hex(u64 val) {
    char buf[19];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        u8 nibble = (val >> (i * 4)) & 0xF;
        buf[2 + (15 - i)] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    buf[18] = '\0';
    print_msg(buf, 18);
}

static void print_u64_dec(u64 val) {
    char buf[21];
    int pos = 20;
    buf[pos--] = '\0';
    if (val == 0) {
        print_msg("0", 1);
        return;
    }
    while (val > 0 && pos >= 0) {
        buf[pos--] = '0' + (val % 10);
        val /= 10;
    }
    u64 len = 0;
    while (buf[pos + 1 + len]) len++;
    print_msg(&buf[pos + 1], len);
}

static int str_eq(const char *a, const char *b, u64 len) {
    for (u64 i = 0; i < len; i++) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

static void parse_gguf_metadata(const u8 *base, u64 max_len) {
    const u8 *limit = base + max_len;
    const u8 *ptr = base + 0x200;

    if (ptr + sizeof(struct GGUFHeader) > limit) return;

    const struct GGUFHeader *hdr = (const struct GGUFHeader *)ptr;
    if (hdr->magic != 0x46554747) return;

    const char m_magic[] = "[+] GGUF HEADER VALID: Magic 'GGUF' (0x46554747)\n";
    print_msg(m_magic, sizeof(m_magic) - 1);

    ptr += sizeof(struct GGUFHeader);
    u64 kv_count = hdr->metadata_kv_count;

    for (u64 i = 0; i < kv_count; i++) {
        if (ptr + 8 > limit) break;
        u64 key_len = *(const u64 *)ptr;
        ptr += 8;

        if (ptr + key_len + 4 > limit) break;
        const char *key_chars = (const char *)ptr;
        ptr += key_len;

        u32 val_type = *(const u32 *)ptr;
        ptr += 4;

        const char arch_key[] = "general.architecture";
        u64 arch_len = sizeof(arch_key) - 1;

        if (key_len == arch_len && str_eq(key_chars, arch_key, arch_len)) {
            if (val_type == GGUF_TYPE_STRING) {
                if (ptr + 8 > limit) break;
                u64 slen = *(const u64 *)ptr;
                ptr += 8;

                if (ptr + slen > limit) break;
                const char m_prefix[] = "[+] GGUF METADATA: general.architecture = ";
                print_msg(m_prefix, sizeof(m_prefix) - 1);
                print_msg((const char *)ptr, slen);
                print_msg("\n", 1);
                return;
            }
        }

        if (val_type <= GGUF_TYPE_INT32 || val_type == GGUF_TYPE_FLOAT32) {
            ptr += 4;
        } else if (val_type == GGUF_TYPE_UINT8 || val_type == GGUF_TYPE_INT8 || val_type == GGUF_TYPE_BOOL) {
            ptr += 1;
        } else if (val_type == GGUF_TYPE_UINT16 || val_type == GGUF_TYPE_INT16) {
            ptr += 2;
        } else if (val_type == GGUF_TYPE_UINT64 || val_type == GGUF_TYPE_INT64 || val_type == GGUF_TYPE_FLOAT64) {
            ptr += 8;
        } else if (val_type == GGUF_TYPE_STRING) {
            if (ptr + 8 > limit) break;
            u64 slen = *(const u64 *)ptr;
            ptr += 8 + slen;
        } else {
            break;
        }
    }
}

s64 linx_main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char m_start[] = "[*] LINX ENGINE: Engaging Substrate via Direct Kernel Syscalls...\n";
    print_msg(m_start, sizeof(m_start) - 1);

    const char path[] = "/home/ron/linx/linum_polyglot.bin";
    s64 fd = (s64)sys_call3(SYS_open, (u64)path, O_RDONLY, 0);
    if (fd < 0) return 1;

    void *addr = (void *)sys_call6(SYS_mmap, 0, SUBSTRATE_SIZE, 
                                  PROT_READ | PROT_WRITE, 
                                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((s64)addr < 0) {
        sys_call3(SYS_close, (u64)fd, 0, 0);
        return 2;
    }

    s64 bytes = (s64)sys_call3(SYS_read, (u64)fd, (u64)addr, SUBSTRATE_SIZE);
    sys_call3(SYS_close, (u64)fd, 0, 0);
    if (bytes != SUBSTRATE_SIZE) return 3;

    const unsigned char *p = (const unsigned char *)addr;
    if (p[0x00] != 'M' || p[0x01] != 'Z' ||
        p[0x40] != 0x7F || p[0x41] != 'E' || p[0x42] != 'L' || p[0x43] != 'F' ||
        p[0x80] != 'P' || p[0x81] != 'E' || p[0x82] != 0 || p[0x83] != 0 ||
        p[0x200] != 'G' || p[0x201] != 'G' || p[0x202] != 'U' || p[0x203] != 'F') {
        return 4;
    }
    const char m_inv[] = "[+] INVARIANTS COMMITTED: [MZ @ 0x00, ELF @ 0x40, PE @ 0x80, GGUF @ 0x200]\n";
    print_msg(m_inv, sizeof(m_inv) - 1);

    /* Phase 1: In-Place GGUF Schema Parsing */
    parse_gguf_metadata(p, SUBSTRATE_SIZE);

    /* Phase 2: Seal page to RX */
    s64 mprot = (s64)sys_call3(SYS_mprotect, (u64)addr, SUBSTRATE_SIZE, PROT_READ | PROT_EXEC);
    if (mprot < 0) return 5;
    const char m_hard[] = "[*] MEMORY HARDENING: W^X Enforced (PROT_READ | PROT_EXEC)\n";
    print_msg(m_hard, sizeof(m_hard) - 1);

    /* Map 4KB linear framebuffer canvas */
    void *fb_canvas = (void *)sys_call6(SYS_mmap, 0, 4096, 
                                        PROT_READ | PROT_WRITE, 
                                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((s64)fb_canvas < 0) return 6;

    u32 *pixels = (u32 *)fb_canvas;
    for (int i = 0; i < 1024; i++) pixels[i] = 0;

    u64 (*vector_entry)(SubstrateContext *, u64) = 
        (u64 (*)(SubstrateContext *, u64))((unsigned char *)addr + 0x3E0);

    /* =========================================================================
     * INFERENCE PASS 1: Baseline Static Vector [1, 2, 3, 4, 5, 6, 7, 8]
     * ========================================================================= */
    SubstrateContext ctx;
    ctx.reserved = 0;
    ctx.stamp = 0;
    ctx.gguf_addr = 0;
    ctx.fb_ptr = (u64)fb_canvas;
    ctx.gemv_checksum = 0;
    ctx._pad = 0;
    ctx.custom_x_ptr = 0;
    ctx.hidden_h = 0;

    u64 tsc1_start = read_tsc();
    u64 rax_res1 = vector_entry(&ctx, 0x4000);
    u64 tsc1_end = read_tsc();

    if (rax_res1 != 0x4C494E55 || ctx.stamp != 0x4D554E494C5F3252 || ctx.gguf_addr != 0x400200) {
        return 7;
    }
    if (ctx.hidden_h != 0x0007000500030001ULL) {
        return 71;
    }

    const char m_p1[] = "[+] PASS 1 (Baseline Vector): Latency = ";
    print_msg(m_p1, sizeof(m_p1) - 1);
    print_u64_dec(tsc1_end - tsc1_start);
    print_msg(" ticks, h = ", 12);
    print_hex(ctx.hidden_h);
    print_msg("\n", 1);

    /* =========================================================================
     * INFERENCE PASS 2: Dynamic Input Vector [2, 2, 2, 2, 2, 2, 2, 2]
     * ========================================================================= */
    u8 custom_x2[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    ctx.custom_x_ptr = (u64)custom_x2;
    u64 tsc2_start = read_tsc();
    u64 rax_res2 = vector_entry(&ctx, 0x4000);
    u64 tsc2_end = read_tsc();

    if (rax_res2 != 0x4C494E55 || ctx.stamp != 0x4D554E494C5F3252) {
        return 8;
    }
    if (ctx.hidden_h != 0x0002000200020002ULL) {
        return 81;
    }

    const char m_p2[] = "[+] PASS 2 (Dynamic Vector): Latency = ";
    print_msg(m_p2, sizeof(m_p2) - 1);
    print_u64_dec(tsc2_end - tsc2_start);
    print_msg(" ticks, h = ", 12);
    print_hex(ctx.hidden_h);
    print_msg("\n", 1);

    /* =========================================================================
     * 10000-ITERATION STEADY-STATE HARDWARE BENCHMARK
     * ========================================================================= */
    u64 total_ticks = 0;
    for (int i = 0; i < 10000; i++) {
        ctx.custom_x_ptr = (i & 1) ? (u64)custom_x2 : 0;
        u64 start = read_tsc();
        vector_entry(&ctx, 0x4000);
        u64 end = read_tsc();
        total_ticks += (end - start);
    }
    u64 mean_ticks = total_ticks / 10000;

    const char m_bench[] = "[+] HARDWARE TELEMETRY: 10000-Run Mean Execution = ";
    print_msg(m_bench, sizeof(m_bench) - 1);
    print_u64_dec(mean_ticks);
    print_msg(" ticks/pass\n", 12);

    /* Framebuffer Scanout Audit */
    int fb_ok = 1;
    for (int i = 0; i < 32; i++) {
        if (pixels[i] != 0x00FF4C49) {
            fb_ok = 0;
            break;
        }
    }
    if (!fb_ok) return 9;

    const char m_fb[] = "[+] FRAMEBUFFER VALIDATION: 32 ARGB pixels verified (Signature 0x00FF4C49)\n";
    print_msg(m_fb, sizeof(m_fb) - 1);

    const char m_done[] = "[*] LINX ENGINE: Execution successful. Exiting 0.\n";
    print_msg(m_done, sizeof(m_done) - 1);

    sys_call3(SYS_munmap, (u64)fb_canvas, 4096, 0);
    sys_call3(SYS_munmap, (u64)addr, SUBSTRATE_SIZE, 0);
    return 0;
}
