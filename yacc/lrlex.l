%option outfile="lex.cpp" header-file="lex.hpp"
%option yylineno

%{
#define YY_DECL yy::parser::symbol_type yylex(void)
#include "parser.hpp"
#include <iostream>
int type;
%}

HEXPREFIX (0x)|(0X)
OCTALPREFIX 0

HEX_FLOAT_TAIL [pP][+-]?{HEXNUM}
DEC_FLOAT_TAIL [eE][+-]?{DECIMALNUM}

OCTALNUM [0-7]
HEXNUM [0-9a-fA-F]
DECIMALNUM [0-9]

HEX {HEXPREFIX}{HEXNUM}*
OCTAL {OCTALPREFIX}{OCTALNUM}*
DECIMAL [1-9]{DECIMALNUM}*

HEX_FLOAT_FORM1 {HEXPREFIX}{HEXNUM}*\.{HEXNUM}+{HEX_FLOAT_TAIL}?
HEX_FLOAT_FORM2 {HEXPREFIX}{HEXNUM}+\.?{HEX_FLOAT_TAIL}
HEX_FLOAT_FORM3 {HEXPREFIX}{HEXNUM}+\.

DEC_FLOAT_FORM1 {DECIMALNUM}*\.{DECIMALNUM}+{DEC_FLOAT_TAIL}?
DEC_FLOAT_FORM2 {DECIMALNUM}+\.?{DEC_FLOAT_TAIL}
DEC_FLOAT_FORM3 {DECIMALNUM}+\.

HEX_FLOAT {HEX_FLOAT_FORM1}|{HEX_FLOAT_FORM2}|{HEX_FLOAT_FORM3}
DEC_FLOAT {DEC_FLOAT_FORM1}|{DEC_FLOAT_FORM2}|{DEC_FLOAT_FORM3}

FLOAT {DEC_FLOAT}|{HEX_FLOAT}

%x COMMENT

%%
"int"       { return yy::parser::make_Y_INT(LocType(yylineno)); }
"float"     { return yy::parser::make_Y_FLOAT(LocType(yylineno)); }
"void"      { return yy::parser::make_Y_VOID(LocType(yylineno)); }
"const"     { return yy::parser::make_Y_CONST(LocType(yylineno)); }
"if"        { return yy::parser::make_Y_IF(LocType(yylineno)); }
"else"      { return yy::parser::make_Y_ELSE(LocType(yylineno)); }
"while"     { return yy::parser::make_Y_WHILE(LocType(yylineno)); }
"break"     { return yy::parser::make_Y_BREAK(LocType(yylineno)); }
"continue"  { return yy::parser::make_Y_CONTINUE(LocType(yylineno)); }
"return"    { return yy::parser::make_Y_RETURN(LocType(yylineno)); }

"+"         { return yy::parser::make_Y_ADD(LocType(yylineno)); }
"-"         { return yy::parser::make_Y_SUB(LocType(yylineno)); }
"*"         { return yy::parser::make_Y_MUL(LocType(yylineno)); }
"/"         { return yy::parser::make_Y_DIV(LocType(yylineno)); }
"%"         { return yy::parser::make_Y_MODULO(LocType(yylineno)); }
"<"         { return yy::parser::make_Y_LESS(LocType(yylineno)); }
"<="        { return yy::parser::make_Y_LESSEQ(LocType(yylineno)); }
">"         { return yy::parser::make_Y_GREAT(LocType(yylineno)); }
">="        { return yy::parser::make_Y_GREATEQ(LocType(yylineno)); }
"!="        { return yy::parser::make_Y_NOTEQ(LocType(yylineno)); }
"=="        { return yy::parser::make_Y_EQ(LocType(yylineno)); }
"!"         { return yy::parser::make_Y_NOT(LocType(yylineno)); }
"&&"        { return yy::parser::make_Y_AND(LocType(yylineno)); }
"||"        { return yy::parser::make_Y_OR(LocType(yylineno)); }
"="         { return yy::parser::make_Y_ASSIGN(LocType(yylineno)); }

"("         { return yy::parser::make_Y_LPAR(LocType(yylineno)); }
")"         { return yy::parser::make_Y_RPAR(LocType(yylineno)); }
"{"         { return yy::parser::make_Y_LBRACKET(LocType(yylineno));}
"}"         { return yy::parser::make_Y_RBRACKET(LocType(yylineno)); }
"["         { return yy::parser::make_Y_LSQUARE(LocType(yylineno)); }
"]"         { return yy::parser::make_Y_RSQUARE(LocType(yylineno)); }
","         { return yy::parser::make_Y_COMMA(LocType(yylineno)); }
";"         { return yy::parser::make_Y_SEMICOLON(LocType(yylineno)); }

{FLOAT} {return yy::parser::make_num_FLOAT(std::stof(yytext),LocType(yylineno));}
{HEX} {return yy::parser::make_num_INT(std::stoi(yytext,nullptr,16),LocType(yylineno));}
{OCTAL} {return yy::parser::make_num_INT(std::stoi(yytext,nullptr,8),LocType(yylineno));}
{DECIMAL} {return yy::parser::make_num_INT(std::stoi(yytext),LocType(yylineno));}

[a-zA-Z_][a-zA-Z0-9_]* { return yy::parser::make_Y_ID(yytext,LocType(yylineno)); }

[ \t\n] ;
"//".*        {}
"/*" {BEGIN COMMENT;}
<COMMENT>"*/" {BEGIN INITIAL;}
<COMMENT>.|\n {}
. {}
<<EOF>>  {return yy::parser::make_YYEOF(LocType(yylineno));}
%%

int yywrap() {
	return 1;
}