.RECIPEPREFIX = >

CC     = clang
LD     = ld.lld
CFLAGS = -target x86_64-unknown-none-elf -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -mgeneral-regs-only -Wall -Wextra
ESP    = ../bootloader/esp

all: kernel.elf

kernel.o: kernel.c
> $(CC) $(CFLAGS) -c kernel.c -o kernel.o

kernel.elf: kernel.o linker.ld
> $(LD) -T linker.ld -o kernel.elf kernel.o

install: kernel.elf
> cp kernel.elf $(ESP)/kernel.elf

clean:
> rm -f kernel.o kernel.elf

.PHONY: all install clean
