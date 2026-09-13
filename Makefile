CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -I.
LDFLAGS ?=
LDLIBS  ?= -lm

MAIN_SRCS := \
    app/main.c \
    app/app_pipeline.c \
    core/frame.c \
    input/vospi_parser.c \
    input/frame_reconstructor.c \
    input/source_emulator.c \
    processing/bad_pixels.c \
    processing/denoise.c \
    processing/normalize.c \
    processing/contrast.c \
    output/output_image.c

.PHONY: all clean sanitize debug

all: main video_to_stream

main: $(MAIN_SRCS)
	$(CC) $(CFLAGS) $(MAIN_SRCS) -o main $(LDFLAGS) $(LDLIBS)

video_to_stream: video_to_stream.c
	$(CC) $(CFLAGS) -O2 -o video_to_stream video_to_stream.c

# Debug build: no optimization, keep symbols
debug: CFLAGS := -Wall -Wextra -O0 -g -I.
debug: clean all

# ASan/UBSan build for catching buffer overruns and undefined behavior
# in the packet parser and frame reconstruction code.
sanitize: CFLAGS := -Wall -Wextra -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer -I.
sanitize: LDFLAGS := -fsanitize=address,undefined
sanitize: clean all

clean:
	rm -f main video_to_stream