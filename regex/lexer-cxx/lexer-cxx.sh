#!/bin/sh

REGEXP="[_a-zA-Z][_a-zA-Z0-9]*"

DIR="folly"
for f in `find $DIR -name "*.cpp"`; do
    echo "*** File $f"
    ./lexer "$REGEXP" $f
done
