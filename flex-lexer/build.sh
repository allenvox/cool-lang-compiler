#!/bin/sh
FLEXDIR=/usr/local/Cellar/flex/2.6.4_2/
flex++ -o CoolLexer.cpp ./CoolLexer.flex
g++ -Wall ./driver.cpp ./CoolLexer.cpp -L$FLEXDIR/lib -I$FLEXDIR/include -o driver