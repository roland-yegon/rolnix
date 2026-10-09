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
    uint32_t type;
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages;
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    UINTN *map_size, void *map, UINTN *map_key,
    UINTN *descriptor_size, uint32_t *descriptor_version);

typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(
    uint32_t pool_type, UINTN size, void **buffer);

struct EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    EFI_GET_MEMORY_MAP get_memory_map;
    EFI_ALLOCATE_POOL allocate_pool;
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

    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    uint32_t desc_version = 0;

    /* Call 1: learn how big the map is (expected to "fail"). */
    bs->get_memory_map(&map_size, 0, &map_key, &desc_size, &desc_version);

    /* Pad for the entries our own allocation will add. */
    map_size += 2 * desc_size;

    /* Pool type 2 = EfiLoaderData, the right type for bootloader data. */
    void *map = 0;
    EFI_STATUS status = bs->allocate_pool(2, map_size, &map);
    if (status != 0) {
        print(out, L"allocate_pool failed: ");
        print_hex(out, status);
        print(out, L"\r\n");
        halt();
    }

    /* Call 2: the real one. */
    status = bs->get_memory_map(&map_size, map, &map_key,
                                &desc_size, &desc_version);
    if (status != 0) {
        print(out, L"get_memory_map failed: ");
        print_hex(out, status);
        print(out, L"\r\n");
        halt();
    }

    UINTN count = map_size / desc_size;
    print(out, L"Memory map entries: ");
    print_u64(out, count);
    print(out, L"\r\n");

    for (UINTN i = 0; i < 10 && i < count; i++) {
        EFI_MEMORY_DESCRIPTOR *d =
            (EFI_MEMORY_DESCRIPTOR *)((uint8_t *)map + i * desc_size);

        print_u64(out, i);
        print(out, L"  type ");
        print_u64(out, d->type);
        print(out, L"  start ");
        print_hex(out, d->physical_start);
        print(out, L"  pages ");
        print_u64(out, d->number_of_pages);
        print(out, L"\r\n");
    }

    halt();
}
