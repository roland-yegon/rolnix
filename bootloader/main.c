#include <stdint.h>

typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;

struct EFI_SYSTEM_TABLE;

EFI_STATUS efi_main(EFI_HANDLE image_handle,
                    struct EFI_SYSTEM_TABLE *system_table) {
  (void)image_handle;
  (void)system_table;
  return 0;
}
