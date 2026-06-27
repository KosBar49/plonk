# Makefile for the field-generic PLONK+FRI prover + verifier (c_core).
#
# Build a self-test binary per field:
#     mingw32-make            (Windows, Strawberry GCC)
#     make                    (Linux/macOS)
#
# Produces: selftest_babybear, selftest_koalabear, selftest_goldilocks
#           prove_babybear,   prove_koalabear,   prove_goldilocks (writes <field>_proof.bin)
#
# Override the max circuit size:  make MAX_LOG_N=10

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra -std=c11 -I.
MAXLOGN ?= 8
CFLAGS  += -DMAX_LOG_N=$(MAXLOGN)

CORE = prover.c verifier.c circuit.c ntt.c merkle.c transcript.c sha256.c
EXE  = .exe

FIELDS = BABYBEAR KOALABEAR GOLDILOCKS

all: selftest prove

selftest:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR   -o selftest_babybear$(EXE)   selftest_main.c $(CORE)
	$(CC) $(CFLAGS) -DFIELD_KOALABEAR  -o selftest_koalabear$(EXE)  selftest_main.c $(CORE)
	$(CC) $(CFLAGS) -DFIELD_GOLDILOCKS -o selftest_goldilocks$(EXE) selftest_main.c $(CORE)

prove:
	$(CC) $(CFLAGS) -DFIELD_BABYBEAR   -o prove_babybear$(EXE)   prove_main.c $(CORE)
	$(CC) $(CFLAGS) -DFIELD_KOALABEAR  -o prove_koalabear$(EXE)  prove_main.c $(CORE)
	$(CC) $(CFLAGS) -DFIELD_GOLDILOCKS -o prove_goldilocks$(EXE) prove_main.c $(CORE)

# Run all three self-tests
test: selftest
	./selftest_babybear$(EXE)
	./selftest_koalabear$(EXE)
	./selftest_goldilocks$(EXE)

clean:
	rm -f *.o *.exe *_proof.bin c_proof.bin ck.bin cg.bin c_out.txt py_out.txt

.PHONY: all selftest prove test clean
