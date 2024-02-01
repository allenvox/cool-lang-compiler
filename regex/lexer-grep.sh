#!/bin/sh

KEYWORDS="(for|if|while)"

# case-insensitive begin
CIB="^(?i)"

IDENTIFIERS="[_a-zA-Z][_a-zA-Z0-9]*"

INTTYPES="(unsigned|long)?\s?(long|long long)?\s?(int)?"
INTLITERALS="(u|l|ll|ul|ull|llu)?"

# numbers
DEC="(-?[1-9][0-9']*|0)"
OCT="-?0[\d']*"
HEX="-?0x[0-9a-f]*"
BINARY="-?0b[01]+"

# left-hand side
INTBEGIN="$INTTYPES $IDENTIFIERS ="

# right-hand side
DECIMAL="$INTBEGIN $DEC$INTLITERALS"
OCTAL="$INTBEGIN $OCT$INTLITERALS"
HEXADECIMAL="$INTBEGIN $HEX$INTLITERALS"
BYTE="$INTBEGIN $BINARY$INTLITERALS"

INT="($DECIMAL|$OCTAL|$HEXADECIMAL|$BYTE)"

# comments
ONELINECOM="//.+"
MULTILINECOM="/\\*(.*?)\\*/"
COMMENTS="($ONELINECOM|$MULTILINECOM)"

# currently running (value to change)
# e.g. : REGEXP="$COMMENTS"
REGEXP="$CIB$INT"

DIR="."
for f in `find $DIR -name "example.cpp"`; do
    echo "*** File $f"
    grep -E -o "$REGEXP" $f
done
