%option noyywrap
%option nounput
%option yylineno
%option noinput
%{
#include <cstdlib>
#include <string>
#include "common/position.h"
#include "sysy.tab.hpp"
using namespace std;
loc current;
int CurCol = 1;
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

"int"           { current.line = yylineno; current.col = CurCol; return Y_INT; CurCol += 3;}
"float"         { current.line = yylineno; current.col = CurCol; return Y_FLOAT; CurCol += 5;}
"void"          { current.line = yylineno; current.col = CurCol; return Y_VOID; CurCol += 4;}
"const"         { current.line = yylineno; current.col = CurCol; return Y_CONST; CurCol += 5;}
"if"            { current.line = yylineno; current.col = CurCol; return Y_IF; CurCol += 2;}
"else"          { current.line = yylineno; current.col = CurCol; return Y_ELSE; CurCol += 4;}
"while"         { current.line = yylineno; current.col = CurCol; return Y_WHILE; CurCol += 5;}
"break"         { current.line = yylineno; current.col = CurCol; return Y_BREAK; CurCol += 5;}
"continue"      { current.line = yylineno; current.col = CurCol; return Y_CONTINUE; CurCol += 8;}
"return"        { current.line = yylineno; current.col = CurCol; return Y_RETURN; CurCol += 6;}

{Id}            { yylval.StrVal = new string(yytext); current.line = yylineno; current.col = yylineno - yyleng + 1; return Y_ID; CurCol += yyleng; }
{String}        { yylval.StrVal = new string(yytext); current.line = yylineno; current.col = yylineno - yyleng + 1; return _string; CurCol += yyleng; }

{Decimal}       { yylval.iVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_INT; CurCol += yyleng; }
{nDecimal}      { yylval.iVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_INT; CurCol += yyleng; }
{Octal}         { yylval.iVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_INT; CurCol += yyleng; }
{nOctal}        { yylval.iVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_INT; CurCol += yyleng; }
{Hexadecimal}   { yylval.iVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_INT; CurCol += yyleng; }
{nHexadecimal}  { yylval.iVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_INT; CurCol += yyleng; }
{Float}         { yylval.fVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_FLOAT; CurCol += yyleng; }
{nFloat}        { yylval.fVal = strtol(yytext, nullptr, 0); current.line = yylineno; current.col = yylineno - yyleng + 1; return num_FLOAT; CurCol += yyleng; }


">"             { current.line = yylineno; current.col = CurCol; return Y_GREAT; CurCol += 1;}
"<"             { current.line = yylineno; current.col = CurCol; return Y_LESS; CurCol += 1;}
">="            { current.line = yylineno; current.col = CurCol; return Y_GREATEQ; CurCol += 2;}
"<="            { current.line = yylineno; current.col = CurCol; return Y_LESSEQ; CurCol += 2;}
"=="            { current.line = yylineno; current.col = CurCol; return Y_EQ; CurCol += 2;}
"!="            { current.line = yylineno; current.col = CurCol; return Y_NOTEQ; CurCol += 2;}

"&&"            { current.line = yylineno; current.col = CurCol; return Y_AND; CurCol += 2;}
"||"            { current.line = yylineno; current.col = CurCol; return Y_OR; CurCol += 2;}

\"[\s\S]*\"     { yylval.StrVal = new string(yytext); current.line = yylineno; current.col = yylineno - yyleng + 1; return _string; CurCol += yyleng; }
\n              { CurCol = 0; }
[ \t\n\r]*      { CurCol++; }
"//".*          {}
"/*"([^*]*\*)([^*/][^*]*\*)*"/"  {}
.               { return yytext[0]; CurCol++; }

%%