# Headless host (x86-64) build of the PS2-patched sources, instrumented with UBSan alignment checks.
# The R5900 (PS2 EE) traps on misaligned loads/stores, so this is how we catch regressions without real hardware.
# Needs gcc, python3 and the SDL2 development files (e.g. libsdl2-dev).  Usage:  make && ./run_replays.sh /path/to/zelda3_assets.dat
# EXTRA=-DZ3_PIXEL_ABGR=1 builds with the PS2's native pixel order (the replay pictures must stay identical).
ROOT := $(abspath ../..)
SRCS := $(filter-out $(ROOT)/src/main.c $(ROOT)/src/opengl.c $(ROOT)/src/glsl_shader.c, $(wildcard $(ROOT)/src/*.c $(ROOT)/snes/*.c)) \
        $(ROOT)/third_party/opus-1.3.1-stripped/opus_decoder_amalgam.c $(ROOT)/ps2/ps2_stubs.c $(ROOT)/ps2/ps2_indexed.c $(ROOT)/ps2/ps2_gsppu.c
OBJS := $(patsubst $(ROOT)/%.c,obj/%.o,$(SRCS)) obj/main_test.o
CFLAGS := -std=gnu11 -O1 -g -fsanitize=alignment -fno-omit-frame-pointer -w -MMD -MP -DSYSTEM_VOLUME_MIXER_AVAILABLE=0 \
          -I$(ROOT) -iquote $(ROOT)/src $(EXTRA) $(shell sdl2-config --cflags)

z3test: $(OBJS)
	gcc -fsanitize=alignment -o $@ $^ $(shell sdl2-config --libs) -lm
main_test.c: make_main_test.py $(ROOT)/src/main.c
	python3 make_main_test.py
obj/main_test.o: main_test.c
	@mkdir -p $(dir $@); gcc $(CFLAGS) -c $< -o $@
obj/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@); gcc $(CFLAGS) -c $< -o $@
clean:
	rm -rf obj z3test main_test.c
-include $(OBJS:.o=.d)
