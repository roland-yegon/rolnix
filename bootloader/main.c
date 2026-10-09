#include <stdint.h>

typedef uint64_t EFI_STATUS;
typedef uint64_t UINTN;
typedef void *EFI_HANDLE;
typedef uint16_t CHAR16;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} EFI_GUID;

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_TEXT_RESET)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *self, uint8_t extended);

typedef EFI_STATUS (*EFI_TEXT_STRING)(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *self, const CHAR16 *string);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET reset;
    EFI_TEXT_STRING output_string;
};

typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;
    uint32_t pixel_information[4];
    uint32_t pixels_per_scan_line;
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

typedef struct {
    uint32_t max_mode;
    uint32_t mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    UINTN size_of_info;
    uint64_t frame_buffer_base;
    UINTN frame_buffer_size;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct {
    void *query_mode;
    void *set_mode;
    void *blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

struct EFI_FILE_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (*open)(struct EFI_FILE_PROTOCOL *self,
                       struct EFI_FILE_PROTOCOL **new_handle,
                       const CHAR16 *name, uint64_t mode,
                       uint64_t attributes);
    EFI_STATUS (*close)(struct EFI_FILE_PROTOCOL *self);
    void *delete_file;
    EFI_STATUS (*read)(struct EFI_FILE_PROTOCOL *self, UINTN *size,
                       void *buffer);
};

struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    EFI_STATUS (*open_volume)(struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *self,
                              struct EFI_FILE_PROTOCOL **root);
};

typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(
    EFI_GUID *protocol, void *registration, void **iface);

typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    UINTN *map_size, void *map, UINTN *map_key,
    UINTN *descriptor_size, uint32_t *descriptor_version);

typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(
    uint32_t pool_type, UINTN size, void **buffer);

typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE image_handle, UINTN map_key);

typedef EFI_STATUS (*EFI_ALLOCATE_PAGES)(
    uint32_t alloc_type, uint32_t memory_type, UINTN pages,
    uint64_t *memory);

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    void *raise_tpl;
    void *restore_tpl;
    EFI_ALLOCATE_PAGES allocate_pages;
    void *free_pages;
    EFI_GET_MEMORY_MAP get_memory_map;
    EFI_ALLOCATE_POOL allocate_pool;
    void *unused1[20]; /* free_pool ... unload_image */
    EFI_EXIT_BOOT_SERVICES exit_boot_services;
    void *unused2[10]; /* get_next_monotonic_count ... locate_handle_buffer */
    EFI_LOCATE_PROTOCOL locate_protocol;
};

struct EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER hdr;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    EFI_HANDLE console_in_handle;
    void *con_in;
    EFI_HANDLE console_out_handle;
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con_out;
    EFI_HANDLE std_err_handle;
    void *std_err;
    void *runtime_services;
    struct EFI_BOOT_SERVICES *boot_services;
};

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

/* The contract with the kernel. Must match kernel/kernel.c exactly. */
struct boot_info {
    uint32_t *fb;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

/* The kernel is built for System V; we are built for Microsoft's ABI.
   This attribute makes clang pass the argument in rdi, not rcx. */
typedef void (__attribute__((sysv_abi)) *kernel_entry_t)(struct boot_info *info);

static void print(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
                  const CHAR16 *s) {
    out->output_string(out, s);
}

static void print_u64(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
                      uint64_t n) {
    CHAR16 buf[21];
    int i = 20;
    buf[i] = 0;
    do {
        buf[--i] = L'0' + (n % 10);
        n /= 10;
    } while (n);
    out->output_string(out, &buf[i]);
}

static void print_hex(struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out,
                      uint64_t n) {
    CHAR16 buf[19];
    buf[0] = L'0';
    buf[1] = L'x';
    for (int i = 0; i < 16; i++) {
        unsigned d = (n >> ((15 - i) * 4)) & 0xF;
        buf[2 + i] = d < 10 ? L'0' + d : L'A' + (d - 10);
    }
    buf[18] = 0;
    out->output_string(out, buf);
}

__attribute__((noreturn)) static void halt(void) {
    for (;;) {
        __asm__ volatile("hlt");
    }
}

__attribute__((noreturn)) static void fail(
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out, const CHAR16 *what,
    EFI_STATUS status) {
    print(out, what);
    print(out, L" failed: ");
    print_hex(out, status);
    print(out, L"\r\n");
    halt();
}

EFI_STATUS efi_main(EFI_HANDLE image_handle,
                    struct EFI_SYSTEM_TABLE *system_table) {
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out = system_table->con_out;
    struct EFI_BOOT_SERVICES *bs = system_table->boot_services;

    /* ---- 1. Read kernel.elf from the ESP ---- */

    static EFI_GUID fs_guid = {
        0x964e5b22, 0x6459, 0x11d2,
        {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

    struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = 0;
    EFI_STATUS status = bs->locate_protocol(&fs_guid, 0, (void **)&fs);
    if (status != 0) {
        fail(out, L"locate file system", status);
    }

    struct EFI_FILE_PROTOCOL *root = 0;
    status = fs->open_volume(fs, &root);
    if (status != 0) {
        fail(out, L"open volume", status);
    }

    struct EFI_FILE_PROTOCOL *kfile = 0;
    status = root->open(root, &kfile, L"kernel.elf", 1, 0);
    if (status != 0) {
        fail(out, L"open kernel.elf", status);
    }

    UINTN buf_size = 65536;
    void *file_buf = 0;
    status = bs->allocate_pool(2, buf_size, &file_buf);
    if (status != 0) {
        fail(out, L"allocate file buffer", status);
    }

    UINTN size = buf_size;
    status = kfile->read(kfile, &size, file_buf);
    if (status != 0) {
        fail(out, L"read kernel.elf", status);
    }

    kfile->close(kfile);
    root->close(root);

    print(out, L"Read ");
    print_u64(out, size);
    print(out, L" bytes\r\n");

    /* ---- 2. Validate the ELF: never trust the file ---- */

    if (size < sizeof(Elf64_Ehdr)) {
        fail(out, L"file too small for ELF header", size);
    }

    Elf64_Ehdr *eh = (Elf64_Ehdr *)file_buf;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L' || eh->e_ident[3] != 'F') {
        fail(out, L"ELF magic", 0);
    }
    if (eh->e_ident[4] != 2) {
        fail(out, L"not ELF64", eh->e_ident[4]);
    }
    if (eh->e_machine != 62) {
        fail(out, L"not x86-64", eh->e_machine);
    }
    if (eh->e_type != 2) {
        fail(out, L"not an EXEC file", eh->e_type);
    }
    if (eh->e_phoff + (uint64_t)eh->e_phnum * eh->e_phentsize > size) {
        fail(out, L"program headers outside file", eh->e_phoff);
    }

    print(out, L"Entry point: ");
    print_hex(out, eh->e_entry);
    print(out, L"\r\n");

    /* ---- 3. Load every PT_LOAD segment to its physical address ---- */

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        Elf64_Phdr *ph = (Elf64_Phdr *)((uint8_t *)file_buf + eh->e_phoff +
                                        (uint64_t)i * eh->e_phentsize);
        if (ph->p_type != 1) {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz) {
            fail(out, L"segment filesz > memsz", i);
        }
        if (ph->p_offset > size || ph->p_filesz > size - ph->p_offset) {
            fail(out, L"segment data outside file", i);
        }

        uint64_t base = ph->p_paddr & ~0xFFFULL;
        uint64_t end = ph->p_paddr + ph->p_memsz;
        uint64_t pages = (end - base + 0xFFF) / 0x1000;

        /* alloc_type 2 = AllocateAddress, memory_type 2 = EfiLoaderData */
        uint64_t addr = base;
        status = bs->allocate_pages(2, 2, pages, &addr);
        if (status != 0) {
            fail(out, L"allocate segment pages", status);
        }

        uint8_t *dst = (uint8_t *)ph->p_paddr;
        uint8_t *src = (uint8_t *)file_buf + ph->p_offset;

        for (uint64_t n = 0; n < ph->p_filesz; n++) {
            dst[n] = src[n];
        }
        for (uint64_t n = ph->p_filesz; n < ph->p_memsz; n++) {
            dst[n] = 0;
        }

        print(out, L"Loaded segment at ");
        print_hex(out, ph->p_paddr);
        print(out, L"\r\n");
    }

    /* ---- 4. Find the framebuffer and fill in boot_info ---- */

    static EFI_GUID gop_guid = {
        0x9042a9de, 0x23dc, 0x4a38,
        {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    status = bs->locate_protocol(&gop_guid, 0, (void **)&gop);
    if (status != 0) {
        fail(out, L"locate graphics output", status);
    }

    static struct boot_info info;
    info.fb = (uint32_t *)gop->mode->frame_buffer_base;
    info.width = gop->mode->info->horizontal_resolution;
    info.height = gop->mode->info->vertical_resolution;
    info.stride = gop->mode->info->pixels_per_scan_line;

    /* Copy the entry point out now: after the exit we want no
       dependence on firmware-owned structures. */
    uint64_t entry = eh->e_entry;

    print(out, L"Handing off to the kernel...\r\n");

    /* ---- 5. Memory map + ExitBootServices ---- */

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_version = 0;

    bs->get_memory_map(&map_size, 0, &map_key, &desc_size, &desc_version);

    UINTN map_buf_size = map_size + 2 * desc_size;
    void *map = 0;
    status = bs->allocate_pool(2, map_buf_size, &map);
    if (status != 0) {
        fail(out, L"allocate memory map buffer", status);
    }

    /* Fetch the map and exit immediately. Retry if the map went stale. */
    int exited = 0;
    for (int tries = 0; tries < 5 && !exited; tries++) {
        map_size = map_buf_size;
        status = bs->get_memory_map(&map_size, map, &map_key,
                                    &desc_size, &desc_version);
        if (status != 0) {
            fail(out, L"get_memory_map", status);
        }
        status = bs->exit_boot_services(image_handle, map_key);
        if (status == 0) {
            exited = 1;
        }
    }

    if (!exited) {
        fail(out, L"ExitBootServices", status);
    }

    /* ---- 6. Boot services are gone. No print(). Jump. ---- */

    kernel_entry_t kmain = (kernel_entry_t)entry;
    kmain(&info);

    halt();
}
