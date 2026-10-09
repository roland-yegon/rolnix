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

EFI_STATUS efi_main(EFI_HANDLE image_handle,
                    struct EFI_SYSTEM_TABLE *system_table) {
    struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *out = system_table->con_out;
    struct EFI_BOOT_SERVICES *bs = system_table->boot_services;

    static EFI_GUID gop_guid = {
        0x9042a9de, 0x23dc, 0x4a38,
        {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};

    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = 0;
    EFI_STATUS status = bs->locate_protocol(&gop_guid, 0, (void **)&gop);
    if (status != 0) {
        print(out, L"No graphics output: ");
        print_hex(out, status);
        print(out, L"\r\n");
        halt();
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *mode = gop->mode;

    /* Copy what we need out of firmware's structs while we still can. */
    uint32_t *fb = (uint32_t *)mode->frame_buffer_base;
    uint32_t width = mode->info->horizontal_resolution;
    uint32_t height = mode->info->vertical_resolution;
    uint32_t stride = mode->info->pixels_per_scan_line;

    /* Before exit: navy screen with an orange rectangle. */
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fb[y * stride + x] = 0x00102040;
        }
    }
    for (uint32_t y = 300; y < 500; y++) {
        for (uint32_t x = 440; x < 840; x++) {
            fb[y * stride + x] = 0x00FF8800;
        }
    }

    /* Get the memory map: first call only learns the size. */
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_version = 0;

    bs->get_memory_map(&map_size, 0, &map_key, &desc_size, &desc_version);

    UINTN buf_size = map_size + 2 * desc_size;
    void *map = 0;
    status = bs->allocate_pool(2, buf_size, &map);
    if (status != 0) {
        print(out, L"allocate_pool failed: ");
        print_hex(out, status);
        print(out, L"\r\n");
        halt();
    }

    /* Fetch the map and exit immediately. Retry if the map went stale. */
    int exited = 0;
    for (int tries = 0; tries < 5 && !exited; tries++) {
        map_size = buf_size;
        status = bs->get_memory_map(&map_size, map, &map_key,
                                    &desc_size, &desc_version);
        if (status != 0) {
            print(out, L"get_memory_map failed: ");
            print_hex(out, status);
            print(out, L"\r\n");
            halt();
        }
        status = bs->exit_boot_services(image_handle, map_key);
        if (status == 0) {
            exited = 1;
        }
    }

    if (!exited) {
        print(out, L"ExitBootServices failed: ");
        print_hex(out, status);
        print(out, L"\r\n");
        halt();
    }

    /* Boot services are gone. No print(), no firmware calls, only us
       and the framebuffer. Draw a green rectangle as proof. */
    for (uint32_t y = 300; y < 500; y++) {
        for (uint32_t x = 900; x < 1100; x++) {
            fb[y * stride + x] = 0x0000CC44;
        }
    }

    halt();
}
