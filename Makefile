CC=gcc

CFLAGS=-c

SDL_CFLAGS= $(shell sdl2-config --cflags)
SDL_LFLAGS= $(shell sdl2-config --libs)

override CFLAGS += $(SDL_CFLAGS)

HEADERDIR= src/
SOURCEDIR= src/

HEADER_FILES=  chip8.h system.h
SOURCE_FILES= main.c chip8.c system.c

HEADERS_FP = $(addprefix $(HEADERDIR),$(HEADER_FILES))
SOURCE_FP = $(addprefix $(SOURCEDIR),$(SOURCE_FILES))

OBJECTS =$(SOURCE_FP:.c = .o)

EXECUTABLE=chip8

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(SDL_LFLAGS) -o $(EXECUTABLE)

%.o: %.c $(HEADERS_FP)
	$(CC) $(CFLAGS) -o $@ $< 

clean:
	rm -rf src/*.o $(EXECUTABLE)