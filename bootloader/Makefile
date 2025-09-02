CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld  
OBJCOPY = riscv64-unknown-elf-objcopy

CFLAGS = -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax

boot.bin: boot.S
	$(CC) $(CFLAGS) -c boot.S -o boot.o
	$(LD) -Ttext 0x80000000 -o boot.elf boot.o
	$(OBJCOPY) -O binary boot.elf boot.bin
	@echo "Boot sector size: $$(wc -c < boot.bin) bytes"

clean:
	rm -f *.o *.elf *.bin

.PHONY: clean
