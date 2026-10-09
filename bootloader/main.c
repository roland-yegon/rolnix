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

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    void *get_memory_map;
    void *allocate_pool;
    void *unused[31]; /* free_pool ... locate_handle_buffer */
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
    (void)image_handle;

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

    print(out, L"Resolution: ");
    print_u64(out, mode->info->horizontal_resolution);
    print(out, L" x ");
    print_u64(out, mode->info->vertical_resolution);
    print(out, L"\r\nPixel format: ");
    print_u64(out, mode->info->pixel_format);
    print(out, L"\r\nPixels per scanline: ");
    print_u64(out, mode->info->pixels_per_scan_line);
    print(out, L"\r\nFramebuffer at: ");
    print_hex(out, mode->frame_buffer_base);
    print(out, L"\r\nFramebuffer size: ");
    print_u64(out, mode->frame_buffer_size);
    print(out, L" bytes\r\n");

    uint32_t *fb = (uint32_t *)mode->frame_buffer_base;
    uint32_t width = mode->info->horizontal_resolution;
    uint32_t height = mode->info->vertical_resolution;
    uint32_t stride = mode->info->pixels_per_scan_line;

    /* Fill the whole screen with dark navy. */
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            fb[y * stride + x] = 0x00102040;
        }
    }

    /* Draw an orange rectangle, 400 wide and 200 tall, in the middle. */
    for (uint32_t y = 300; y < 500; y++) {
        for (uint32_t x = 440; x < 840; x++) {
            fb[y * stride + x] = 0x00FF8800;
        }
    }

    halt();
}
