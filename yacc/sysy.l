%option noyywrap
%option nounput
%option yylineno
%option noinput
%{
#include <cstdlib>
#include <string>
#include "../include/lib/position.hpp"
#include "sysy.tab.hpp"
using namespace std;
loc current;
int CurCol = 1;

void updatePosition(int length) {
    current.line = yylineno;
    current.col = CurCol;
    CurCol += length;
}

%}

Id   [a-zA-Z_][a-zA-Z0-9_]*  
String        \"([^\"\\]|\\.)*\"  

Decimal        [1-9][0-9]*  
nDecimal       -[1-9][0-9]*  
Octal          0[0-7]*  
nOctal         -0[0-7]*  
Hexadecimal    0[xX][0-9a-fA-F]+  
nHexadecimal   -0[xX][0-9a-fA-F]+  
Float          [0-9]+"."[0-9]+  
nFloat         -[0-9]+"."[0-9]+  

%%

"int"           { updatePosition(3); return Y_INT; }
"float"         { updatePosition(5); return Y_FLOAT; }
"void"          { updatePosition(4); return Y_VOID; }
"const"         { updatePosition(5); return Y_CONST; }
"if"            { updatePosition(2); return Y_IF; }
"else"          { updatePosition(4); return Y_ELSE; }
"while"         { updatePosition(5); return Y_WHILE; }
"break"         { updatePosition(5); return Y_BREAK; }
"continue"      { updatePosition(8); return Y_CONTINUE; }
"return"        { updatePosition(6); return Y_RETURN; }

{Id}            { yylval.StrVal = new string(yytext); updatePosition(yyleng); return Y_ID; }
{String}        { yylval.StrVal = new string(yytext); updatePosition(yyleng); return _string; }

{Decimal}       { yylval.iVal = strtol(yytext, nullptr, 0); updatePosition(yyleng); return num_INT; }
{nDecimal}      { yylval.iVal = strtol(yytext, nullptr, 0); updatePosition(yyleng); return num_INT; }
{Octal}         { yylval.iVal = strtol(yytext, nullptr, 0); updatePosition(yyleng); return num_INT; }
{nOctal}        { yylval.iVal = strtol(yytext, nullptr, 0); updatePosition(yyleng); return num_INT; }
{Hexadecimal}   { yylval.iVal = strtol(yytext, nullptr, 0); updatePosition(yyleng); return num_INT; }
{nHexadecimal}  { yylval.iVal = strtol(yytext, nullptr, 0); updatePosition(yyleng); return num_INT; }
{Float}         { yylval.fVal = strtof(yytext, nullptr); updatePosition(yyleng); return num_FLOAT; }
{nFloat}        { yylval.fVal = strtof(yytext, nullptr); updatePosition(yyleng); return num_FLOAT; }

">"             { updatePosition(1); return Y_GREAT; }
"<"             { updatePosition(1); return Y_LESS; }
">="            { updatePosition(2); return Y_GREATEQ; }
"<="            { updatePosition(2); return Y_LESSEQ; }
"=="            { updatePosition(2); return Y_EQ; }
"!="            { updatePosition(2); return Y_NOTEQ; }

"&&"            { updatePosition(2); return Y_AND; }
"||"            { updatePosition(2); return Y_OR; }

[ \t\r]+        { CurCol += yyleng; }
\n              { CurCol = 1; }

"//".*          {}
"/*"([^*]|\*+[^*/])*\*+"/"  {}

.               { return yytext[0]; CurCol++; }

%%
