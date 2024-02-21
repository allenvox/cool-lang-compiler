#pragma once

#include <iostream>
#include <fstream>

#undef yyFlexLexer
#include <FlexLexer.h>

class CoolLexer : public yyFlexLexer {
private:
    std::ostream& out;
    int lineno;
    int comment_level;
    void Error(const char* msg) const;

public:
    CoolLexer(std::istream& arg_yyin, std::ostream& arg_yyout) :
        yyFlexLexer{arg_yyin, arg_yyout}, out{arg_yyout}, lineno{1}, comment_level{0} {}
    virtual int yylex();
};
