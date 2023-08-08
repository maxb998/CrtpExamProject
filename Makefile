# compiler name
CC = clang

SRC_DIR := src/
BIN_DIR = bin/
CFLAGS = -Wall -g

# files list
SOURCES := $(wildcard $(SRC_DIR)*.c)
OUTPUTS := $(SOURCES:$(SRC_DIR)%.c=$(BIN_DIR)%)

# command used to check variables value and "debug" the makefile
print:
	@echo SOURCES = $(SOURCES)
	@echo OUTPUTS = $(OUTPUTS)

# build options when debugging
build: $(OUTPUTS)

$(BIN_DIR)%: $(SRC_DIR)%.c
	$(CC) $(CFLAGS) $(SRC_DIR)$(*F).c -o $@

# delete all gcc output files
clean:
	rm -f bin/main
