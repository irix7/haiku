# M0 test images for the SGI Indy.
#
#   hello.ecoff   prints "muse: m0 alive" on the serial console
#   rex3.ecoff    the same, then draws through REX3 and prints
#                 "muse: m0 rex3 done"

TOOLCHAIN ?= /mnt/europa/toolchains/mips64-n32--uclibc--stable-2026.08-1/bin/mips64-buildroot-linux-uclibc-
MIPS_CC ?= $(TOOLCHAIN)gcc
MIPS_LD ?= $(TOOLCHAIN)ld
OUT ?= ../../generated.mips/m0

CFLAGS = -mabi=n32 -march=mips3 -mtune=r4600 -EB -msoft-float -mno-abicalls \
	-fno-pic -G0 -ffreestanding -nostdlib -O2 -Wall
LDFLAGS = -m elf32btsmipn32 -T link.ld
ELF2ECOFF = python3 ../../build/scripts/elf2ecoff.py

all: $(OUT)/hello.ecoff $(OUT)/rex3.ecoff

$(OUT):
	mkdir -p $(OUT)

$(OUT)/start.o: start.S | $(OUT)
	$(MIPS_CC) $(CFLAGS) -c $< -o $@

$(OUT)/hello.o: hello.c | $(OUT)
	$(MIPS_CC) $(CFLAGS) -c $< -o $@

$(OUT)/rex3.o: hello.c | $(OUT)
	$(MIPS_CC) $(CFLAGS) -DREX3_TEST -c $< -o $@

$(OUT)/%.elf: $(OUT)/start.o $(OUT)/%.o link.ld
	$(MIPS_LD) $(LDFLAGS) $(OUT)/start.o $(OUT)/$*.o -o $@

$(OUT)/%.ecoff: $(OUT)/%.elf
	$(ELF2ECOFF) $< $@

clean:
	rm -rf $(OUT)

.PHONY: all clean
.SECONDARY:
