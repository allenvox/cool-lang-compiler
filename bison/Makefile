DIRGUARD = @mkdir -p $(@D)

BIN = bin
OBJ = obj
SRC = src

CFLAGS = -Wall
CC = gcc

ANALYZERY = $(SRC)/infixcalc.y
ANALYZERC = $(OBJ)/infixcalc.c
EXE = $(BIN)/infixcalc

.PHONY: all
all: $(EXE)
$(EXE): $(ANALYZERC)
	$(DIRGUARD)
	$(CC) $(CFLAGS) $^ -o $@ -lm

.PHONY: $(ANALYZERC)
$(ANALYZERC): $(ANALYZERY)
	$(DIRGUARD)
	bison -o $@ $<

.PHONY: clean
.SILENT: clean
clean:
	rm -rf $(BIN) $(OBJ)