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

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
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
    (void)image_handle;

    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out = system_table->con_out;
    struct EFI_BOOT_SERVICES *bs = system_table->boot_services;

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

    uint64_t header = 0;
    UINTN size = sizeof(header);
    status = kfile->read(kfile, &size, &header);
    if (status != 0) {
        fail(out, L"read kernel.elf", status);
    }

    print(out, L"Read ");
    print_u64(out, size);
    print(out, L" bytes: ");
    print_hex(out, header);
    print(out, L"\r\n");

    uint8_t *b = (uint8_t *)&header;
    if (b[0] == 0x7F && b[1] == 'E' && b[2] == 'L' && b[3] == 'F') {
        print(out, L"ELF magic: OK\r\n");
    } else {
        print(out, L"ELF magic: BAD\r\n");
    }

    kfile->close(kfile);
    root->close(root);

    halt();
}
