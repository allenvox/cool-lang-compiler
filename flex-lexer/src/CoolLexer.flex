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
string            \"([^\n]|\'\')*\"

%x COMMENT

%option warn nodefault batch noyywrap c++
%option yylineno
%option yyclass="CoolLexer"

%%

"(*"               BEGIN(COMMENT);
<COMMENT>[^}\n]+   { /* skip*/ }
<COMMENT>\n        { lineno++; }
<COMMENT><<EOF>>   Error("EOF in comment");
<COMMENT>"*)"      BEGIN(INITIAL);

class              return TOKEN_CLASS;
else               return TOKEN_ELSE;
fi                 return TOKEN_FI;
if                 return TOKEN_IF;
in                 return TOKEN_IN;
inherits           return TOKEN_INHERITS;
let                return TOKEN_LET;
loop               return TOKEN_LOOP;
pool               return TOKEN_POOL;
then               return TOKEN_THEN;
while              return TOKEN_WHILE;
case               return TOKEN_CASE;
esac               return TOKEN_ESAC;
new                return TOKEN_NEW;
isvoid             return TOKEN_ISVOID;
of                 return TOKEN_OF;
not                return TOKEN_NOT;
"{"                return TOKEN_BLOCKOPEN;
"}"                return TOKEN_BLOCKCLOSE;
{string}           return TOKEN_STRING;
{identifier}       return TOKEN_IDENTIFIER;

[*/+\-,^.;:()\[\]] return yytext[0];
{white_space}      { /* skip spaces */ }
\n                 lineno++;
.                  Error("unrecognized character");

%%

void CoolLexer::Error(const char* msg) const {
    std::cerr << "Lexer error (line " << lineno << "): " << msg << ": lexeme '" << YYText() << "'\n";
    std::exit(YY_EXIT_FAILURE);
}
