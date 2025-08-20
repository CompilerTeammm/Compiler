%option outfile="scanner_output.cpp" header-file="scanner_output.hpp"
%option yylineno

%{
#include <iostream>
#include "parser_output.hpp"
#define YY_DECL yy::parser::symbol_type yylex()
%}

/* 标识符 */
id    ([a-zA-Z_])([a-zA-Z0-9_])*

/* 关键字映射 */
KW_INT       int
KW_FLOAT     float
KW_CONST     const
KW_WHILE     while
KW_IF        if
KW_ELSE      else
KW_CONTINUE  continue
KW_BREAK     break
KW_VOID      void
KW_RETURN    return

/* 数字前缀 */
HEX_HEAD     (0x|0X)
OCT_HEAD     0

/* 数值部分 */
DIGIT_OCT    [0-7]
DIGIT_HEX    [0-9a-fA-F]
DIGIT_DEC    [0-9]

/* 浮点尾部 */
HEX_TAIL     [pP][+-]?{DIGIT_HEX}
DEC_TAIL     [eE][+-]?{DIGIT_DEC}

/* 整数常量 */
HEX_NUM      {HEX_HEAD}{DIGIT_HEX}*
OCT_NUM      {OCT_HEAD}{DIGIT_OCT}*
DEC_NUM      [1-9]{DIGIT_DEC}*

/* 浮点形式 */
HEX_1       {HEX_HEAD}{DIGIT_HEX}*\.{DIGIT_HEX}+{HEX_TAIL}?
HEX_2       {HEX_HEAD}{DIGIT_HEX}+\.?{HEX_TAIL}
HEX_3       {HEX_HEAD}{DIGIT_HEX}+\.

DEC_1       {DIGIT_DEC}*\.{DIGIT_DEC}+{DEC_TAIL}?
DEC_2       {DIGIT_DEC}+\.?{DEC_TAIL}
DEC_3       {DIGIT_DEC}+\.

/* 浮点汇总 */
HEX_FLOAT    {HEX_1}|{HEX_2}|{HEX_3}
DEC_FLOAT    {DEC_1}|{DEC_2}|{DEC_3}
NUM_FLOAT    {DEC_FLOAT}|{HEX_FLOAT}

/* 运算符符号 */
OP_ADD       "+"
OP_SUB       "-"
OP_MUL       "*"
OP_DIV       "/"
OP_MOD       "%"
OP_GT        ">"
OP_GTE       ">="
OP_LT        "<"
OP_LTE       "<="
OP_EQ        "=="
OP_ASSIGN    "="
OP_NEQ       "!="
OP_NOT       "!"
OP_AND       "&&"
OP_OR        "||"

/* 符号 */
SYM_LCB      "{"
SYM_RCB      "}"
SYM_LSQ      "["
SYM_RSQ      "]"
SYM_LP       "("
SYM_RP       ")"
SYM_SEMI     ";"
SYM_COMMA    ","
SYM_SQUOTE   "\'"
SYM_DQUOTE   "\""
SYM_HASH     "#"

/* 注释状态 */
%x MULTI_COMMENT

%%

{KW_INT}         { return yy::parser::make_Y_INT(); }
{KW_FLOAT}       { return yy::parser::make_Y_FLOAT(); }
{KW_VOID}        { return yy::parser::make_Y_VOID(); }
{KW_CONST}       { return yy::parser::make_Y_CONST(); }
{KW_BREAK}       { return yy::parser::make_Y_BREAK(); }
{KW_WHILE}       { return yy::parser::make_Y_WHILE(); }
{KW_IF}          { return yy::parser::make_Y_IF(); }
{KW_ELSE}        { return yy::parser::make_Y_ELSE(); }
{KW_RETURN}      { return yy::parser::make_Y_RETURN(); }
{KW_CONTINUE}    { return yy::parser::make_Y_CONTINUE(); }

{id}     { return yy::parser::make_Y_ID(yytext); }

{NUM_FLOAT}      { return yy::parser::make_num_FLOAT(std::stof(yytext)); }
{HEX_NUM}        { return yy::parser::make_num_INT(std::stoi(yytext, nullptr, 16)); }
{OCT_NUM}        { return yy::parser::make_num_INT(std::stoi(yytext, nullptr, 8)); }
{DEC_NUM}        { return yy::parser::make_num_INT(std::stoi(yytext)); }

{OP_ADD}         { return yy::parser::make_Y_ADD(); }
{OP_SUB}         { return yy::parser::make_Y_SUB(); }
{OP_MUL}         { return yy::parser::make_Y_MUL(); }
{OP_MOD}         { return yy::parser::make_Y_MODULO(); }
{OP_DIV}         { return yy::parser::make_Y_DIV(); }
{OP_GT}          { return yy::parser::make_Y_GREAT(); }
{OP_GTE}         { return yy::parser::make_Y_GREATEQ(); }
{OP_LT}          { return yy::parser::make_Y_LESS(); }
{OP_LTE}         { return yy::parser::make_Y_LESSEQ(); }
{OP_EQ}          { return yy::parser::make_Y_EQ(); }
{OP_ASSIGN}      { return yy::parser::make_Y_ASSIGN(); }
{OP_NEQ}         { return yy::parser::make_Y_NOTEQ(); }
{OP_NOT}         { return yy::parser::make_Y_NOT(); }
{OP_AND}         { return yy::parser::make_Y_AND(); }
{OP_OR}          { return yy::parser::make_Y_OR(); }

{SYM_LP}         { return yy::parser::make_Y_LPAR(); }
{SYM_RP}         { return yy::parser::make_Y_RPAR(); }
{SYM_LSQ}        { return yy::parser::make_Y_LSQUARE(); }
{SYM_RSQ}        { return yy::parser::make_Y_RSQUARE(); }
{SYM_LCB}        { return yy::parser::make_Y_LBRACKET(); }
{SYM_RCB}        { return yy::parser::make_Y_RBRACKET(); }
{SYM_SEMI}       { return yy::parser::make_Y_SEMICOLON(); }
{SYM_COMMA}      { return yy::parser::make_Y_COMMA(); }

<<EOF>>          { return yy::parser::make_YYEOF(); }

"/*"             { BEGIN MULTI_COMMENT; }
<MULTI_COMMENT>"*/"    { BEGIN INITIAL; }
<MULTI_COMMENT>.|\n    {}
"//".*           {}
.|\n             {}

%%

int yywrap() {
    return 1;
}