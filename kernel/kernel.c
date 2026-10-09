#include <stdint.h>

struct boot_info {
    uint32_t *fb;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
};

__attribute__((noreturn)) void kmain(struct boot_info *info) {
    for (uint32_t y = 0; y < info->height; y++) {
        for (uint32_t x = 0; x < info->width; x++) {
            info->fb[y * info->stride + x] = 0x00601080;
        }
    }

    for (;;) {
        __asm__ volatile("hlt");
    }
}
