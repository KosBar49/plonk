# Unit tests + cross-checks for the C core.
# Run from this directory:  mingw32-make   (Windows)   /   make   (Linux/macOS)
#
# Each test builds against the implementation one level up (../).
# The cross-check Python scripts live in crosscheck/ and compare C output
# against the Python reference in ../../python.

CC     := gcc
CFLAGS := -O2 -Wall -Wextra -std=c11 -I..
SRC    := ../ntt.c ../merkle.c ../transcript.c ../sha256.c ../circuit.c
EXE    := .exe

all: field ntt merkle setup transcript

field:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR -o test_field$(EXE) test_field.c
	./test_field$(EXE)

ntt:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR -o test_ntt$(EXE) test_ntt.c ../ntt.c
	./test_ntt$(EXE)

merkle:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR -o test_merkle$(EXE) test_merkle.c ../merkle.c ../sha256.c
	./test_merkle$(EXE)

setup:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR -o test_setup$(EXE) test_setup.c ../circuit.c ../ntt.c
	./test_setup$(EXE)

transcript:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR -o test_transcript$(EXE) test_transcript.c ../transcript.c ../sha256.c
	./test_transcript$(EXE)

clean:
	rm -f *.exe *.o

.PHONY: all field ntt merkle setup transcript clean
