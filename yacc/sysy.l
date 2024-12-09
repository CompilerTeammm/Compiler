%option noyywrap
%option nounput
%option noinput
%option yylineno

%{
#include <cstdlib>
#include <string>
#include "common/position.h"
#include "sysy.tab.hpp"
using namespace std;

loc cur_pos;
int col_record = 1;
%}

WhiteSpace    [ \t\n\r]*
LineComment   "//".*
BlockComment  "/*"([^*]*\*)([^*/][^*]*\*)*"/"

Identifier    [a-zA-Z_][a-zA-Z0-9_]*
String        \"([^\"\\]|\\.)*\"

Decimal       [1-9][0-9]*
Octal         0[0-7]*
Hexadecimal   0[xX][0-9a-fA-F]+

%%

"int"           { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_INT; col_record += 3;}
"void"          { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_VOID; col_record += 4;}
"const"         { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_CONST; col_record += 5;}
"if"            { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_IF; col_record += 2;}
"else"          { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_ELSE; col_record += 4;}
"while"         { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_WHILE; col_record += 5;}
"break"         { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_BREAK; col_record += 5;}
"continue"      { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_CONTINUE; col_record += 8;}
"return"        { cur_pos.line = yylineno; cur_pos.col = col_record; return Y_RETURN; col_record += 6;}

{Identifier}    { yylval.str_val = new string(yytext); cur_pos.line = yylineno; cur_pos.col = yylineno - yyleng + 1; return _identifier; col_record += yyleng; }
{String}        { yylval.str_val = new string(yytext); cur_pos.line = yylineno; cur_pos.col = yylineno - yyleng + 1; return _string; col_record += yyleng; }

{Decimal}       { yylval.int_val = strtol(yytext, nullptr, 0); cur_pos.line = yylineno; cur_pos.col = yylineno - yyleng + 1; return _const_val; col_record += yyleng; }
{Octal}         { yylval.int_val = strtol(yytext, nullptr, 0); cur_pos.line = yylineno; cur_pos.col = yylineno - yyleng + 1; return _const_val; col_record += yyleng; }
{Hexadecimal}   { yylval.int_val = strtol(yytext, nullptr, 0); cur_pos.line = yylineno; cur_pos.col = yylineno - yyleng + 1; return _const_val; col_record += yyleng; }

">"             { cur_pos.line = yylineno; cur_pos.col = col_record; return _greater; col_record += 1;}
"<"             { cur_pos.line = yylineno; cur_pos.col = col_record; return _less; col_record += 1;}
">="            { cur_pos.line = yylineno; cur_pos.col = col_record; return _greater_equal; col_record += 2;}
"<="            { cur_pos.line = yylineno; cur_pos.col = col_record; return _less_equal; col_record += 2;}
"=="            { cur_pos.line = yylineno; cur_pos.col = col_record; return _equal; col_record += 2;}
"!="            { cur_pos.line = yylineno; cur_pos.col = col_record; return _nequal; col_record += 2;}

"&&"            { cur_pos.line = yylineno; cur_pos.col = col_record; return _logical_and; col_record += 2;}
"||"            { cur_pos.line = yylineno; cur_pos.col = col_record; return _logical_or; col_record += 2;}

\"[\s\S]*\"     { yylval.str_val = new string(yytext); cur_pos.line = yylineno; cur_pos.col = yylineno - yyleng + 1; return _string; col_record += yyleng; }
\n              { col_record = 0; }
{WhiteSpace}    { col_record++; }
{LineComment}   { }
{BlockComment}  { }
.               { return yytext[0]; col_record ++; }

%%