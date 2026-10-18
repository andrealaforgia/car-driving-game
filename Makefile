.DEFAULT_GOAL := all

CC = gcc

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S), Linux)
    INSTALL_CMD = sudo apt-get update && sudo apt-get install -y libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
    DEV_INSTALL_CMD = sudo apt-get update && sudo apt-get install -y cpplint clang-format
else ifeq ($(UNAME_S), Darwin)
    INSTALL_CMD = brew install sdl2 sdl2_image sdl2_mixer sdl2_ttf
    DEV_INSTALL_CMD = brew install cpplint clang-format
else
    $(error Unsupported platform)
endif

SDL2_CFLAGS := $(shell sdl2-config --cflags)
SDL2_LFLAGS := $(shell sdl2-config --libs) -lSDL2_image -lSDL2_mixer -lSDL2_ttf

ENGINE_DIRS = $(wildcard engine/core/*)
GAME_DIRS = $(wildcard game/src/*)

SRC = $(foreach dir,$(ENGINE_DIRS) $(GAME_DIRS),$(wildcard $(dir)/*.c))
HEADERS = $(foreach dir,$(ENGINE_DIRS) $(GAME_DIRS),$(wildcard $(dir)/*.h))
GAME_SRC = $(foreach dir,$(GAME_DIRS),$(wildcard $(dir)/*.c))
GAME_HEADERS = $(foreach dir,$(GAME_DIRS),$(wildcard $(dir)/*.h))

OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)

-include $(DEP)

INCLUDES = $(foreach dir,$(ENGINE_DIRS) $(GAME_DIRS),-I$(dir))

CFLAGS := -ggdb3 -O3 --std=c99 -Wall -Wextra -pedantic-errors $(INCLUDES) $(SDL2_CFLAGS)
LFLAGS := $(SDL2_LFLAGS) -lm

TARGET = car-driving
SNAPSHOT = render_snapshot
SOUND = render_sound

.PHONY: all install dev_install clean lint format test sanitize snapshot sound

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(LFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(SNAPSHOT): $(filter-out game/src/main/main.o,$(OBJ)) tests/render_snapshot.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

snapshot: $(SNAPSHOT)
	./$(SNAPSHOT) snapshot.bmp

$(SOUND): $(filter-out game/src/main/main.o,$(OBJ)) tests/render_sound.c
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

sound: $(SOUND)
	./$(SOUND) engine.wav

install:
	git submodule update --init --recursive
	$(INSTALL_CMD)

dev_install:
	git submodule update --init --recursive
	$(DEV_INSTALL_CMD)

lint:
	cpplint --filter=-build/include_subdir,-legal/copyright,-runtime/threadsafe_fn --root=engine $(GAME_SRC) $(GAME_HEADERS) tests/*.c

format:
	clang-format -i -style=Google $(GAME_SRC) $(GAME_HEADERS) tests/*.c

test:
	python3 tests/run_tests.py

sanitize:
	python3 tests/run_tests.py --sanitize

clean:
	rm -f $(OBJ) $(DEP) $(TARGET) $(SNAPSHOT) $(SOUND) snapshot.bmp engine.wav
