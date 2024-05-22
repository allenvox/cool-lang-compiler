#!/bin/sh

./build.sh
echo "\n\033[92;1mClasses test\033[0m"
bin/analyzer tests/classes.cl
echo "\n\033[92;1mCorrect program test\033[0m"
bin/analyzer tests/correct.cl
echo "\n\033[92;1mDuplicates test\033[0m"
bin/analyzer tests/duplicates.cl
echo "\n\033[92;1mInherits test\033[0m"
bin/analyzer tests/inherits.cl
echo "\n\033[92;1mTypes test\033[0m"
bin/analyzer tests/types.cl
