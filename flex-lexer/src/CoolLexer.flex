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

white_space               [ \t\f\b\r]*
digit                     [0-9]
alpha                     [A-Za-z_]
alpha_num                 ({alpha}|{digit})
identifier                {alpha}{alpha_num}*
string                    \"([^\"\n]|\\\n)*\"
bad_string                \"([^\"\n]|\\\n)*
onelinecom                --.*

%x COMMENT

%option warn nodefault batch noyywrap c++
%option yylineno
%option yyclass="CoolLexer"

%%

"*)"                      Error("Unmatched comment ending");
"(*"                      BEGIN(COMMENT);
<COMMENT>"(*"             { comment_level++; }
<COMMENT><<EOF>>          Error("EOF in comment");
<COMMENT>\n               { lineno++; }
<COMMENT>.                { }
<COMMENT>"*)"             {
                            if (comment_level == 0) {
                                BEGIN(INITIAL);
                            }
                            comment_level--;
                          }
{onelinecom}              { }

t(?i:rue)                 return TOKEN_TRUE;
f(?i:alse)                return TOKEN_FALSE;
(?i:class)                return TOKEN_CLASS;
(?i:else)                 return TOKEN_ELSE;
(?i:fi)                   return TOKEN_FI;
(?i:if)                   return TOKEN_IF;
(?i:in)                   return TOKEN_IN;
(?i:inherits)             return TOKEN_INHERITS;
(?i:let)                  return TOKEN_LET;
(?i:loop)                 return TOKEN_LOOP;
(?i:pool)                 return TOKEN_POOL;
(?i:then)                 return TOKEN_THEN;
(?i:while)                return TOKEN_WHILE;
(?i:case)                 return TOKEN_CASE;
(?i:esac)                 return TOKEN_ESAC;
(?i:new)                  return TOKEN_NEW;
(?i:isvoid)               return TOKEN_ISVOID;
(?i:of)                   return TOKEN_OF;
(?i:not)                  return TOKEN_NOT;
"<="                      return TOKEN_LEQ;
">="                      return TOKEN_GEQ;
"<-"                      return TOKEN_ASSIGN;
"=>"                      return TOKEN_ARROW;
{string}                  return TOKEN_STRING;
{bad_string}              Error("non-terminated string");
[a-z_]{alpha_num}*        return TOKEN_IDENTIFIER;
[A-Z]{alpha_num}*         return TOKEN_TYPE;
{digit}+                  return TOKEN_INT;

[<>=@*/+\-,^.;:~()\[\]{}] return yytext[0];

{white_space}             { }
\n                        lineno++;
.                         Error("unrecognized character");

%%

void CoolLexer::Error(const char* msg) const {
    std::cerr << "Lexer error (line " << lineno << "): " << msg << ": lexeme '" << YYText() << "'\n";
    std::exit(YY_EXIT_FAILURE);
}
