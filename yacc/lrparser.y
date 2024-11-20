%{
    #include <stdio.h>
    #include "ast.h"
    int yylex(void);
    void yyerror(char *s);
    extern int type;
%}

%union
{
    int iValue;
    float fValue;
    char* idValue;
    past pAst;
};

%token <iValue> num_INT Y_INT Y_FLOAT Y_VOID Y_CONST Y_IF Y_ELSE Y_WHILE Y_BREAK Y_CONTINUE Y_RETURN 
%token <iValue> Y_ADD Y_SUB Y_MUL Y_DIV Y_MODULO Y_LESS Y_LESSEQ Y_GREAT Y_GREATEQ Y_NOTEQ Y_EQ Y_NOT Y_AND Y_OR Y_ASSIGN
%token <iValue> Y_LPAR Y_RPAR Y_LBRACKET Y_RBRACKET Y_LSQUARE Y_RSQUARE Y_COMMA Y_SEMICOLON
%token <fValue> num_FLOAT
%token <idValue> Y_ID 
%type  <pAst>	CompUnit Decl ConstDecl ConstDefs ConstDef ConstExps ConstInitVal ConstInitVals
%type  <pAst>	VarDecl VarDecls VarDef InitVal InitVals FuncDef FuncParams FuncParam Block BlockItems BlockItem
%type  <pAst>	Stmt Stmtt Exp LVal ArraySubscripts PrimaryExp UnaryExp CallParams MulExp AddExp RelExp EqExp LAndExp LOrExp ConstExp Type 
%start Start

%%
Start: CompUnit               {showAst($1,0);}
      ;
CompUnit: VarDef CompUnit      {past l=newCompUnit($1,$2);$$=l;}
         |ConstDecl CompUnit   {past l=newCompUnit($1,$2);$$=l;}
         |FuncDef CompUnit     {past l=newCompUnit($1,$2);$$=l;}
         |Decl                 {$$=newCompUnit($1,NULL);}
         |FuncDef              {$$=newCompUnit($1,NULL);}
         |VarDef               {$$=newCompUnit($1,NULL);}
         ;

Decl:ConstDecl
     |VarDecl
     ;

ConstDecl: Y_CONST Type ConstDef Y_SEMICOLON     {$$ = $3;} 
          | Y_CONST Type ConstDefs Y_SEMICOLON   {$$ = $3;}
          ;

ConstDefs: ConstDef Y_COMMA ConstDef     {$$ = newNode($1, $3);}
          | ConstDefs Y_COMMA ConstDef   {$$ = newNode($1, $3);}
          ;

ConstDef: Y_ID Y_ASSIGN ConstInitVal              {$$ = newVarDecl(type, $1, NULL, $3);}
         | Y_ID ConstExps Y_ASSIGN ConstInitVal   {$$ = newVarDecl(type, $1, NULL, $4); }
         ; 

ConstExps: Y_LSQUARE ConstExp Y_RSQUARE              {$$ = $2;}
          | Y_LSQUARE ConstExp Y_RSQUARE ConstExps   {$$ = $2; }
          ;

ConstInitVal: ConstExp
             | Y_LBRACKET Y_RBRACKET                                 {$$ = newIntList(NULL,NULL); }
             | Y_LBRACKET ConstInitVal Y_RBRACKET                    {$$ = newIntList($2,NULL);}
             | Y_LBRACKET ConstInitVal ConstInitVals Y_RBRACKET      {$$ = newIntList($2,$3); }
             ;

ConstInitVals: Y_COMMA ConstInitVal                          {$$ = $2;}
              | Y_COMMA ConstInitVal ConstInitVals           {$$ = newNode($2,$3); }
              ;  

VarDecl: Type VarDef Y_SEMICOLON                       {$$ = newDeclStmt(NULL, $2);} 
        | Type VarDef VarDecls Y_SEMICOLON             {$$ = newDeclStmt($2, $3);}
        ;
 
VarDecls: Y_COMMA VarDef                 {$$ = $2;}
         | Y_COMMA VarDef VarDecls       {$$ = newNode($2, $3);}
         ;

VarDef: Y_ID                                     {$$ = newVarDecl(type, $1, NULL, NULL);}
       | Y_ID Y_ASSIGN InitVal                   {$$ = newVarDecl(type, $1, NULL, $3);}
       | Y_ID ConstExps                          {$$ = newVarDecl(type, $1, NULL, NULL); }
       | Y_ID ConstExps Y_ASSIGN InitVal         {$$ = newVarDecl(type, $1, NULL, $4); }
       | Type Y_ID Y_ASSIGN InitVal Y_SEMICOLON  {$$ = newVarDecl(type, $2, NULL, $4);}
       | Type Y_ID Y_SEMICOLON                   {$$ = newVarDecl(type, $2, NULL, NULL);}
       | Type Y_ID ConstExps Y_SEMICOLON         {$$ = newVarDecl(type, $2, NULL, NULL);}
       ; 

InitVal: Exp
        | Exp Y_COMMA Exp                                   {$$ = newIntList($1,$3);}
        | Y_LBRACKET Y_RBRACKET                             {$$ = newIntList(NULL,NULL);}
        | Y_LBRACKET Exp Y_RBRACKET                         {$$ = newIntList($2,NULL);}
        | Y_LBRACKET Exp Y_COMMA Exp Y_RBRACKET             {$$ = newIntList($2,$4);}
        | Y_LBRACKET InitVals Y_COMMA InitVals Y_RBRACKET   {$$ = newIntList(newNode($2,$4),NULL) ; }
        ;

InitVals: InitVal Y_COMMA InitVal   {$$ = newNode($1,$3); }
         ;

FuncDef: Type Y_ID Y_LPAR Y_RPAR Block               {$$ = newFuncDecl($1->ivalue,$2, NULL, newCompoundStmt(NULL, $5));}
        | Type Y_ID Y_LPAR FuncParams Y_RPAR Block   {$$ = newFuncDecl($1->ivalue, $2, $4, newCompoundStmt(NULL, $6));}    
        ;

FuncParams: FuncParam 
           | FuncParams Y_COMMA FuncParam    {$$=newNode($1,$3);}
           ;

FuncParam: Type Y_ID                                         {$$ = newParaDecl( $2, NULL, NULL);}
          | Type Y_ID Y_LSQUARE Y_RSQUARE                    {$$ = newParaDecl( $2, NULL, NULL);}
          | Type Y_ID ArraySubscripts                        {$$ = newParaDecl( $2, NULL, NULL);}
          | Type Y_ID Y_LSQUARE Y_RSQUARE ArraySubscripts    {$$ = newParaDecl( $2, NULL, NULL);}
          ;

Block: Y_LBRACKET BlockItems Y_RBRACKET    {$$ = $2;}
      | Y_LBRACKET Y_RBRACKET              {$$ = NULL;}
      ;

BlockItems: BlockItem              {$$ = newCompoundStmt($1, NULL);}
           | BlockItem BlockItems  {past l = newCompoundStmt($1, $2);$$ = l;}
           ;

BlockItem: Decl
          | Stmt 
          ;

Stmt: LVal Y_ASSIGN Exp Y_SEMICOLON                   {$$ = newBinaryOper("=", Y_ASSIGN, $1, $3);}
     | Y_SEMICOLON                                    {$$ = NULL;}
     | Exp Y_SEMICOLON                                {$$ = $1;}
     | Block 
     | Y_WHILE Y_LPAR LOrExp Y_RPAR Stmt              {$$ = newWhileStmt($3, newCompoundStmt(NULL,$5));}
     | Y_IF Y_LPAR LOrExp Y_RPAR Block                {$$ = newIfStmt($3, newCompoundStmt(NULL,$5), NULL);}
     | Y_IF Y_LPAR LOrExp Y_RPAR Block Y_ELSE Block   {$$ = newIfStmt($3, newCompoundStmt(NULL,$5), newCompoundStmt(NULL,$7));}
     | Y_IF Y_LPAR LOrExp Y_RPAR Stmtt                {$$ = newIfStmt($3, $5, NULL);}
     | Y_IF Y_LPAR LOrExp Y_RPAR Stmtt Y_ELSE Stmtt   {$$ = newIfStmt($3, $5, $7);}
     | Y_IF Y_LPAR LOrExp Y_RPAR Stmtt Y_ELSE Block   {$$ = newIfStmt($3, $5, newCompoundStmt(NULL,$7));}
     | Y_IF Y_LPAR LOrExp Y_RPAR Block Y_ELSE Stmtt   {$$ = newIfStmt($3, newCompoundStmt(NULL,$5), $7);}
     | Y_BREAK Y_SEMICOLON                            {$$ = newBreakStmt();}
     | Y_CONTINUE Y_SEMICOLON                         {$$ = newContinueStmt();}
     | Y_RETURN Exp Y_SEMICOLON                       {$$ = newReturnStmt($2, NULL);}
     | Y_RETURN Y_SEMICOLON                           {$$ = newReturnStmt(NULL, NULL);}
     ;

Stmtt: LVal Y_ASSIGN Exp Y_SEMICOLON    {$$ = newBinaryOper("=", Y_ASSIGN, $1, $3);}
      | Y_SEMICOLON                     {$$ = NULL;}
      | Exp Y_SEMICOLON                 {$$ = $1;}
      | Block 
      | Y_WHILE Y_LPAR LOrExp Y_RPAR Stmt                {$$ = newWhileStmt($3, newCompoundStmt(NULL,$5));}
      | Y_IF Y_LPAR LOrExp Y_RPAR Block                  {$$ = newIfStmt($3, newCompoundStmt(NULL,$5), NULL);}
      | Y_IF Y_LPAR LOrExp Y_RPAR Block Y_ELSE Block     {$$ = newIfStmt($3, newCompoundStmt(NULL,$5), newCompoundStmt(NULL,$7));}
      | Y_IF Y_LPAR LOrExp Y_RPAR Stmtt                  {$$ = newIfStmt($3, $5, NULL);}
      | Y_IF Y_LPAR LOrExp Y_RPAR Stmtt Y_ELSE Stmtt     {$$ = newIfStmt($3, $5, $7);}
      | Y_IF Y_LPAR LOrExp Y_RPAR Stmtt Y_ELSE Block     {$$ = newIfStmt($3, $5, newCompoundStmt(NULL,$7));}
      | Y_IF Y_LPAR LOrExp Y_RPAR Block Y_ELSE Stmtt     {$$ = newIfStmt($3, newCompoundStmt(NULL,$5), $7);}
      | Y_BREAK Y_SEMICOLON                              {$$ = newBreakStmt();}
      | Y_CONTINUE Y_SEMICOLON                           {$$ = newContinueStmt();}
      | Y_RETURN Exp Y_SEMICOLON                         {$$ = newReturnStmt($2, NULL);}
      | Y_RETURN Y_SEMICOLON                             {$$ = newReturnStmt(NULL, NULL);}

Exp: AddExp  
    ;

LVal: Y_ID               {$$ = newDeclRefExp($1, NULL, NULL);}
     | ArraySubscripts   {$$ = $1;}
     ;

ArraySubscripts: ArraySubscripts Y_LSQUARE Exp Y_RSQUARE    {$$=newArraySubscriptsExp($1, $3);}
                | Y_ID Y_LSQUARE Exp Y_RSQUARE              {$$=newArraySubscriptsExp(newDeclRefExp($1,NULL,NULL), $3);}
                ;

PrimaryExp: Y_LPAR Exp Y_RPAR        {$$ = newParenExp($2);}
           | LVal 
           | num_INT                 {$$ = newIntVal($1);}
           | num_FLOAT               {$$ = newFloatVal($1);}
           ;

UnaryExp: PrimaryExp
         | LVal Y_LPAR Y_RPAR              {$$ = newCallExp(0, NULL, $1, NULL);}
         | LVal Y_LPAR CallParams Y_RPAR   {$$ = newCallExp(0, NULL, $1, $3);}
         | Y_ADD UnaryExp                  {$$ = newBinaryOper("+", Y_ADD, NULL, $2);}
         | Y_SUB UnaryExp                  {$$ = newBinaryOper("-", Y_SUB, NULL, $2);}
         | Y_NOT UnaryExp                  {$$ = newBinaryOper("!", Y_NOT, NULL, $2);}
         ;

CallParams: Exp
           | Exp Y_COMMA CallParams   {$$ = newNode($1, $3);}
           ;


MulExp: UnaryExp
       | MulExp Y_MUL UnaryExp     {$$ = newBinaryOper("*", Y_MUL, $1, $3);}
       | MulExp Y_DIV UnaryExp     {$$ = newBinaryOper("/", Y_DIV, $1, $3);}
       | MulExp Y_MODULO UnaryExp  {$$ = newBinaryOper("%", Y_MODULO, $1, $3);}
       ;

AddExp: MulExp
       | AddExp Y_ADD MulExp   {$$ = newBinaryOper("+", Y_ADD, $1, $3);}
       | AddExp Y_SUB MulExp   {$$ = newBinaryOper("-", Y_SUB, $1, $3);}
       ;

RelExp: AddExp
       | AddExp Y_LESS RelExp     {$$ = newBinaryOper("<", Y_LESS, $1, $3);}
       | AddExp Y_GREAT RelExp    {$$ = newBinaryOper(">", Y_GREAT, $1, $3);}
       | AddExp Y_LESSEQ RelExp   {$$ = newBinaryOper("<=", Y_LESSEQ, $1, $3);}
       | AddExp Y_GREATEQ RelExp  {$$ = newBinaryOper(">=", Y_GREATEQ, $1, $3);}
       ;

EqExp: RelExp
      | RelExp Y_EQ EqExp      {$$ = newBinaryOper("==", Y_EQ, $1, $3);}
      | RelExp Y_NOTEQ EqExp   {$$ = newBinaryOper("!=", Y_NOTEQ, $1, $3);}
      ;

LAndExp: EqExp 
        | EqExp Y_AND LAndExp  {$$ = newBinaryOper("&&", Y_AND, $1, $3);}
        ;

LOrExp: LAndExp
       | LAndExp Y_OR LOrExp  {$$ = newBinaryOper("||", Y_OR, $1, $3);}
       ;

ConstExp: AddExp
          ;
               
Type: Y_INT      {yylval.iValue = Y_INT;$$ = newType(Y_INT);}
     | Y_FLOAT   {yylval.iValue = Y_FLOAT;$$ = newType(Y_FLOAT);}
     | Y_VOID    {yylval.iValue = Y_VOID;$$ = newType(Y_VOID);}
     ;

%%