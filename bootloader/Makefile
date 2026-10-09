.RECIPEPREFIX = >

CC      = clang
LD      = lld-link
CFLAGS  = -target x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone -Wall -Wextra
LDFLAGS = -subsystem:efi_application -entry:efi_main -nodefaultlib

OVMF_CODE = /usr/share/edk2/x64/OVMF_CODE.4m.fd
EFI       = esp/EFI/BOOT/BOOTX64.EFI

all: $(EFI)

main.o: main.c
> $(CC) $(CFLAGS) -c main.c -o main.o

$(EFI): main.o
> $(LD) $(LDFLAGS) -out:$(EFI) main.o

run: $(EFI)
> qemu-system-x86_64 -machine q35 -m 256M \
>   -drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
>   -drive if=pflash,format=raw,file=OVMF_VARS.4m.fd \
>   -drive format=raw,file=fat:rw:esp \
>   -net none

clean:
> rm -f main.o $(EFI)

.PHONY: all run clean
