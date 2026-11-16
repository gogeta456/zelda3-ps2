IOP_BIN = audsrv.irx
IOP_SRC_DIR = src/
IOP_INC_DIR = src/
IOP_OBJS = audsrv.o upsamplers.o hw.o rpc_server.o rpc_client.o common.o cdrom.o imports.o exports.o adpcm.o
IOP_CFLAGS += -MMD -MP
all: $(IOP_BIN)
clean:
	rm -rf obj audsrv.irx audsrv.notiopmod.elf audsrv.notiopmod.stripped.elf
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.iopglobal
-include $(IOP_OBJS:.o=.d)
