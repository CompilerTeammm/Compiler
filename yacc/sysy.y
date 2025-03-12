
%code requires {
  #include <memory>
  #include <string>
  #include "../include/lib/ast/AstNode.hpp"
  #include "../include/lib/position.h"
  #include "../include/lib/Singleton.hpp"
}

%{
#include <iostream>
#include <memory>
#include <string>
#include "../include/lib/ast/AstNode.hpp"
#include "../include/lib/position.h"
BaseAST *root;
extern loc current;
using namespace std;

int yylex();
void yyerror(BaseAST* &ast, const char *s);
void pos(BaseAST* &node){
    node->position.line = current.line; 
    node->position.col = current.col;
}
%}

%union
{
    std::string *StrVal;
    VarType TypeVal;
    int iVal;
    float fVal;
    BaseAST *AstVal;
    InitValTree<BaseAST*>* InitVal;
}

%parse-param { BaseAST* &ast }

//bison 版本
%require "3.2"
//语言
%language "c++" 
//语义值存储的方式(C++中Union不是很方便)
%define api.value.type variant
//要求Bison生成make_NUMBER这种函数(如果没有这个,在flex中构造函数错误写错了可能可以过编译)
%define api.token.constructor
//生成header,flex需要用Bison++自动定义的类
%header "parser.hpp"
%output "parser.cpp"
%locations
%define api.location.type {loc}
// %no-lines
//token的enum前缀

%token Y_INT
%token Y_FLOAT
%token Y_VOID
%token Y_CONST
%token Y_BREAK
%token Y_WHILE
%token Y_IF
%token Y_ELSE
%token Y_RETURN
%token Y_CONTINUE
%token <std::string> Y_ID
%token <float> num_FLOAT
%token <int> num_INT
%token Y_ADD
%token Y_SUB
%token Y_MUL
%token Y_MODULO
%token Y_DIV
%token Y_GREAT
%token Y_GREATEQ
%token Y_LESS
%token Y_LESSEQ
%token Y_EQ
%token Y_ASSIGN
%token Y_NOTEQ
%token Y_NOT
%token Y_AND
%token Y_OR
%token Y_LPAR
%token Y_RPAR
%token Y_LSQUARE
%token Y_RSQUARE
%token Y_LBRACKET
%token Y_RBRACKET
%token Y_SEMICOLON
%token Y_COMMA

%nterm <CompUnit*> CompUnit
%nterm <Stmt*> Decl
%nterm <ConstDecl*> ConstDecl
%nterm <ConstDefs*> ConstDefs
%nterm <ConstDef*> ConstDef
%nterm <Exps*> ConstExps
%nterm <InitVal*> ConstInitVal
%nterm <InitVals*> ConstInitVals
%nterm <InitVal*> InitVal
%nterm <InitVals*> InitVals
%nterm <VarDecl*> VarDecl
%nterm <VarDefs*> VarDefs
%nterm <VarDef*> VarDef
%nterm <FuncDef*> FuncDef
%nterm <FuncParams*> FuncParams
%nterm <FuncParam*> FuncParam
%nterm <Block*> Block
%nterm <BlockItems*> BlockItems
%nterm <Stmt*> BlockItem
%nterm <Stmt*> Stmt
%nterm <LVal*> LVal
%nterm <HasOperand*> PrimaryExp
%nterm <CallParams*> CallParams
%nterm <UnaryExp*> UnaryExp
%nterm <MulExp*> MulExp
%nterm <AddExp*> AddExp
%nterm <RelExp*> RelExp
%nterm <EqExp*> EqExp
%nterm <LAndExp*> LAndExp
%nterm <LOrExp*> LOrExp
%nterm <AST_Type> Type
%nterm <Exps*> ArraySubscripts

%start entry
%%
entry:CompUnit {Singleton<CompUnit*>()=$1;}

CompUnit: Decl CompUnit {$$=$2;$$->push_front((AST_NODE*)$1);$$->position=@1->position;}
        | FuncDef CompUnit {$$=$2;$$->push_front((AST_NODE*)$1);$$->position=@1->position;}
        | Decl {$$=new CompUnit((AST_NODE*)$1);$$->position=@1->position;}
        | FuncDef {$$=new CompUnit((AST_NODE*)$1);$$->position=@1->position;}
        ;

Decl: ConstDecl {$$=(Stmt*)$1;$$->position=@1->position;}
    | VarDecl {$$=(Stmt*)$1;$$->position=@1->position;}
    ;

ConstDecl: Y_CONST Type ConstDefs Y_SEMICOLON {$$=new ConstDecl($2,$3);$$->position=@1->position;}
         ;

ConstDefs: ConstDefs Y_COMMA ConstDef {$$=$1;$1->push_back($3);$$->position=@1->position;}
         | ConstDef {$$=new ConstDefs($1);$$->position=@1->position;}
         ;

ConstDef: Y_ID Y_ASSIGN ConstInitVal {$$=new ConstDef($1,nullptr,$3);$$->position=@1->position;}
        | Y_ID ConstExps Y_ASSIGN ConstInitVal {$$=new ConstDef($1,$2,$4);$$->position=@1->position;}
        ;

ConstExps: Y_LSQUARE AddExp Y_RSQUARE {$$=new Exps($2);$$->position=@1->position;}
         | Y_LSQUARE AddExp Y_RSQUARE ConstExps {$$=$4;$$->push_front($2);$$->position=@1->position;}
         ;

ConstInitVal: AddExp {$$=new InitVal((AST_NODE*)$1);$$->position=@1->position;}
            | Y_LBRACKET Y_RBRACKET {$$=new InitVal();$$->position=@1->position;}
            | Y_LBRACKET ConstInitVals Y_RBRACKET {$$=new InitVal((AST_NODE*)$2);$$->position=@1->position;}
            ;

ConstInitVals: ConstInitVal {$$=new InitVals($1);$$->position=@1->position;}
             | ConstInitVals Y_COMMA ConstInitVal {$$=$1;$$->push_back($3);$$->position=@1->position;}
             ;

VarDecl: Type VarDefs Y_SEMICOLON {$$=new VarDecl($1,$2);$$->position=@1->position;}
       ;

VarDefs: VarDef {$$=new VarDefs($1);$$->position=@1->position;}
       | VarDefs Y_COMMA VarDef {$$=$1;$$->push_back($3);$$->position=@1->position;}
       ;

VarDef: Y_ID {$$=new VarDef($1);$$->position=@1->position;}
      | Y_ID Y_ASSIGN InitVal {$$=new VarDef($1,nullptr,$3);$$->position=@1->position;}
      | Y_ID ConstExps {$$=new VarDef($1,$2,nullptr);$$->position=@1->position;}
      | Y_ID ConstExps Y_ASSIGN InitVal {$$=new VarDef($1,$2,$4);$$->position=@1->position;}
      ;

InitVal: AddExp {$$=new InitVal((AST_NODE*)$1);$$->position=@1->position;}
       | Y_LBRACKET Y_RBRACKET {$$=new InitVal(nullptr);$$->position=@1->position;}
       | Y_LBRACKET InitVals Y_RBRACKET {$$=new InitVal((AST_NODE*)$2);$$->position=@1->position;}
       ;

InitVals: InitVal {$$=new InitVals($1);$$->position=@1->position;}
        | InitVals Y_COMMA InitVal {$$=$1;$$->push_back($3);$$->position=@1->position;}
        ;

FuncDef: Type Y_ID Y_LPAR Y_RPAR Block {$$=new FuncDef($1,$2,nullptr,$5);$$->position=@1->position;}
       | Type Y_ID Y_LPAR FuncParams Y_RPAR Block {$$=new FuncDef($1,$2,$4,$6);$$->position=@1->position;}
       ; 

FuncParams: FuncParam {$$=new FuncParams($1);$$->position=@1->position;}
          | FuncParams Y_COMMA FuncParam {$$=$1;$$->push_back($3);$$->position=@1->position;}
          ;

FuncParam: Type Y_ID {$$=new FuncParam($1,$2);$$->position=@1->position;}
         | Type Y_ID Y_LSQUARE Y_RSQUARE {$$=new FuncParam($1,$2,true);$$->position=@1->position;}
         | Type Y_ID ArraySubscripts {$$=new FuncParam($1,$2,false,$3);$$->position=@1->position;}
         | Type Y_ID Y_LSQUARE Y_RSQUARE ArraySubscripts {$$=new FuncParam($1,$2,true,$5);$$->position=@1->position;}
         ;

Block: Y_LBRACKET BlockItems Y_RBRACKET {$$=new Block($2);$$->position=@1->position;}
     | Y_LBRACKET Y_RBRACKET {$$=new Block(nullptr);$$->position=@1->position;}
     ;

BlockItems: BlockItem {$$=new BlockItems($1);$$->position=@1->position;}
          | BlockItems BlockItem {$$=$1;$$->push_back($2);$$->position=@1->position;}
          ;

BlockItem: Decl {$$=(Stmt*)$1;$$->position=@1->position;}
         | Stmt {$$=(Stmt*)$1;$$->position=@1->position;}
         ;

Stmt: LVal Y_ASSIGN AddExp Y_SEMICOLON {$$=new AssignStmt($1,$3);$$->position=@1->position;}
    | Y_SEMICOLON {$$=new ExpStmt(nullptr);$$->position=@1->position;}
    | AddExp Y_SEMICOLON {$$=new ExpStmt($1);$$->position=@1->position;}
    | Block {$$=$1;$$->position=@1->position;}
    | Y_WHILE Y_LPAR LOrExp Y_RPAR Stmt {$$=new WhileStmt($3,$5);$$->position=@1->position;}
    | Y_IF Y_LPAR LOrExp Y_RPAR Stmt Y_ELSE Stmt {$$=new IfStmt($3,$5,$7);$$->position=@1->position;}
    | Y_IF Y_LPAR LOrExp Y_RPAR Stmt {$$=new IfStmt($3,$5);$$->position=@1->position;}
    | Y_BREAK Y_SEMICOLON {$$=new BreakStmt();$$->position=@1->position;}
    | Y_CONTINUE Y_SEMICOLON {$$=new ContinueStmt();$$->position=@1->position;}
    | Y_RETURN AddExp Y_SEMICOLON {$$=new ReturnStmt($2);$$->position=@1->position;}
    | Y_RETURN Y_SEMICOLON {$$=new ReturnStmt();$$->position=@1->position;}
    ;

LVal: Y_ID {$$=new LVal($1);$$->position=@1->position;}
    | Y_ID ArraySubscripts {$$=new LVal($1,$2);$$->position=@1->position;}
    ;

ArraySubscripts: Y_LSQUARE AddExp Y_RSQUARE {$$=new Exps($2);$$->position=@1->position;}
               | Y_LSQUARE AddExp Y_RSQUARE ArraySubscripts {$$=$4;$$->push_front($2);$$->position=@1->position;}
               ;

PrimaryExp: Y_LPAR AddExp Y_RPAR {$$=(HasOperand*)$2;$$->position=@1->position;}
          | LVal {$$=(HasOperand*)$1;$$->position=@1->position;}
          | num_INT {$$=(HasOperand*)(new ConstValue<int>($1));$$->position=@1->position;}
          | num_FLOAT {$$=(HasOperand*)(new ConstValue<float>($1));$$->position=@1->position;}
          | Y_ID Y_LPAR Y_RPAR {$$=(HasOperand*)(new FunctionCall($1));$$->position=@1->position;}
          | Y_ID Y_LPAR CallParams Y_RPAR {$$=(HasOperand*)(new FunctionCall($1,$3));$$->position=@1->position;}
          ;

UnaryExp: PrimaryExp {$$=new UnaryExp($1);$$->position=@1->position;}
        | Y_ADD UnaryExp {$$=$2;$$->push_front(AST_ADD);$$->position=@1->position;}
        | Y_SUB UnaryExp {$$=$2;$$->push_front(AST_SUB);$$->position=@1->position;}
        | Y_NOT UnaryExp {$$=$2;$$->push_front(AST_NOT);$$->position=@1->position;}
        ;

CallParams: AddExp {$$=new CallParams($1);$$->position=@1->position;}
          | AddExp Y_COMMA CallParams {$$=$3;$$->push_front($1);$$->position=@1->position;}
          ;

MulExp: UnaryExp {$$=new MulExp($1);$$->position=@1->position;}
      | MulExp Y_MUL UnaryExp {$$=$1;$$->push_back(AST_MUL);$$->push_back($3);$$->position=@1->position;}
      | MulExp Y_DIV UnaryExp {$$=$1;$$->push_back(AST_DIV);$$->push_back($3);$$->position=@1->position;}
      | MulExp Y_MODULO UnaryExp {$$=$1;$$->push_back(AST_MODULO);$$->push_back($3);$$->position=@1->position;}

AddExp: MulExp {$$=new AddExp($1);$$->position=@1->position;}
      | AddExp Y_ADD MulExp {$$=$1;$$->push_back(AST_ADD);$$->push_back($3);$$->position=@1->position;}
      | AddExp Y_SUB MulExp {$$=$1;$$->push_back(AST_SUB);$$->push_back($3);$$->position=@1->position;}

RelExp: AddExp {$$=new RelExp($1);$$->position=@1->position;}
      | AddExp Y_LESS RelExp {$$=$3;$$->push_front(AST_LESS);$$->push_front($1);$$->position=@1->position;}
      | AddExp Y_GREAT RelExp {$$=$3;$$->push_front(AST_GREAT);$$->push_front($1);$$->position=@1->position;}
      | AddExp Y_LESSEQ RelExp {$$=$3;$$->push_front(AST_LESSEQ);$$->push_front($1);$$->position=@1->position;}
      | AddExp Y_GREATEQ RelExp {$$=$3;$$->push_front(AST_GREATEQ);$$->push_front($1);$$->position=@1->position;}

EqExp: RelExp {$$=new EqExp($1);$$->position=@1->position;}
     | RelExp Y_EQ EqExp {$$=$3;$$->push_front(AST_EQ);$$->push_front($1);$$->position=@1->position;}
     | RelExp Y_NOTEQ EqExp {$$=$3;$$->push_front(AST_NOTEQ);$$->push_front($1);$$->position=@1->position;}

LAndExp: EqExp {$$=new LAndExp($1);$$->position=@1->position;}
       | EqExp Y_AND LAndExp {$$=$3;$$->push_front(AST_AND);$$->push_front($1);$$->position=@1->position;}

LOrExp: LAndExp {$$=new LOrExp($1);$$->position=@1->position;}
      | LAndExp Y_OR LOrExp {$$=$3;$$->push_front(AST_OR);$$->push_front($1);$$->position=@1->position;}

Type: Y_INT {$$=AST_INT;}
    | Y_FLOAT {$$=AST_FLOAT;}
    | Y_VOID {$$=AST_VOID;}

%%