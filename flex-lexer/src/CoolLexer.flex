%{
#include <iostream>
#include <fstream>
#include <cstdio>
#include <string>

#include "Parser.h"
#include "CoolLexer.h"

#undef YY_DECL
#define YY_DECL int CoolLexer::yylex()

%}

white_space       [ \t]*
digit             [0-9]
alpha             [A-Za-z_]
alpha_num         ({alpha}|{digit})
hex_digit         [0-9a-fA-F]
identifier        {alpha}{alpha_num}*
unsigned_integer  {digit}+
hex_integer       ${hex_digit}{hex_digit}*
exponent          e[+-]?{digit}+
i                 {unsigned_integer}
real              ({i}\.{i}?|{i}?\.{i}){exponent}?
string            \'([^'\n]|\'\')*\'
bad_string        \'([^'\n]|\'\')*

%x COMMENT

%option warn nodefault batch noyywrap c++
%option yylineno
%option yyclass="CoolLexer"

%%

. std::cout << "unrecognized character\n";

%%
