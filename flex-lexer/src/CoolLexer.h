#pragma once

#include <iostream>
#include <fstream>

#undef yyFlexLexer
#include <FlexLexer.h>

class CoolLexer : public yyFlexLexer {
private:
    std::ostream& out;
    void Error(const char* msg) const;
    void Escape() const;

public:
    CoolLexer(std::istream& arg_yyin, std::ostream& arg_yyout) :
        yyFlexLexer{arg_yyin, arg_yyout}, out{arg_yyout} {}
    virtual int yylex();
};
