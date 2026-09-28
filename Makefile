# world.execute (me); -- terminal ASCII music video
#
#   make            build ./start
#   make run        build and play
#   make check      run the built-in selftest (timeline + every effect)
#   make preview    render PNG/GIF stills of the show into preview/
#   make data       regenerate src/lyrics_data.h from the .lrc / .yrc
#   make font       regenerate src/font_data.h from a system TTF
#   make clean      remove build products

CC      ?= cc
CFLAGS  ?= -O2 -std=gnu11 -Wall -Wextra -Wpedantic -Wshadow
LDLIBS  ?= -lm

SRC := src/main.c src/term.c src/gfx.c src/lyrics.c src/scenes.c \
       src/fx_world.c src/fx_love.c src/audio.c src/sprites.c
OBJ := $(SRC:.c=.o)
HDR := $(wildcard src/*.h)

BIN := start

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

%.o: %.c $(HDR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN)

check: $(BIN)
	./$(BIN) --selftest

preview: $(BIN)
	python3 tools/mvpreview.py --bin ./$(BIN) --from 0 --to 20000 --step 160 --scale 1 \
	        --frames preview/frames --out preview/png --gif preview/opening.gif
	python3 tools/mvpreview.py --bin ./$(BIN) --scale 2 --frames preview/story \
	        --out preview/story --sheet preview/storyboard.png \
	        --at 500,2500,6500,21000,34000,48000,66000,75000,86000,96000,108000,127000,149000,159500,178000,190000,196000,209000

data:
	python3 tools/gen_lyrics.py

font:
	python3 tools/gen_font.py

clean:
	rm -f $(OBJ) $(BIN)

distclean: clean
	rm -rf preview

.PHONY: all run check preview data font clean distclean