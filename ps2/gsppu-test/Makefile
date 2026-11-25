# PS2 side:  make -C ps2/gsppu-test  (needs PS2DEV/PS2SDK/GSKIT in the environment)
ROOT := $(abspath ../..)
CC := mips64r5900el-ps2-elf-gcc
CFLAGS := $(XFLAGS) -D_EE -G0 -O2 -Wall -std=gnu11 -w -I$(ROOT) -I$(ROOT)/src -I. -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I$(PS2SDK)/ports/include -I$(GSKIT)/include
gsppu_test.elf: gsppu_test.c ../ps2_gsppu.c
	$(CC) $(CFLAGS) -T$(PS2SDK)/ee/startup/linkfile -o $@ $^ -L$(PS2SDK)/ee/lib -L$(GSKIT)/lib -Wl,-zmax-page-size=128 -lgskit -ldmakit -ldma -lgraph -lpacket -ldraw -lc -lkernel -lm
