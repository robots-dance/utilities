TRAFF_PLAYER=trpl.exe

CC=cl
CFLAGS+=/TP

BUILD_PROGRAM=$(CC) $(CFLAGS) $^

all: $(TRAFF_PLAYER)

$(TRAFF_PLAYER): trpl.c 
	$(BUILD_PROGRAM) /link ws2_32.lib

clean:
	del /Q $(TRAFF_PLAYER) 2>nul
	del /Q *.obj 2>nul

