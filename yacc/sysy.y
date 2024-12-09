
%code requires {
  #include <memory>
  #include <string>
  #include "ast/AST_NODE.hpp"
  #include "common/position.h"
  #include "common/initvalT.h"
}

%{
#include <iostream>
#include <memory>
#include <string>
#include "ast/AST_NODE.hpp"
#include "common/position.h"
#include "common/initvalT.h"
BaseAST *root;
extern loc cur_pos;
using namespace std;

int yylex();
void yyerror(BaseAST* &ast, const char *s);
void pos(BaseAST* &node){
    node->position.line = cur_pos.line; 
    node->position.col = cur_pos.col;
}
%}

%union
{
    std::string *str_val;
    VarType type_val;
    int int_val;
    BaseAST *ast_val;
    std::vector<int> *int_vec_val;
    std::vector<BaseAST*> *ast_vec_val;
    InitValTree<BaseAST*>* init_val;
    InitValTree<int>* const_init_val;
}

%parse-param { BaseAST* &ast }

%token Y_INT Y_VOID Y_CONST
%token Y_IF Y_ELSE Y_WHILE Y_BREAK Y_CONTINUE Y_RETURN
%token _equal _nequal _greater _less _greater_equal _less_equal _logical_and _logical_or
%token <str_val> _identifier _string 
%token <int_val> _const_val
%type<type_val> BType
%type <ast_val> CompUnit Compunit FuncDef  Block block BlockItem Stmt FuncFParam Decl ConstDecl VarDecl Vardecl Vardef VarDef LVal Exp UnaryExp PrimaryExp Number UnaryOp AddExp MulExp RelExp EqExp LAndExp LOrExp FuncFParams  Funcfparam FuncRParams  Cond String
Constdecl Constdef ConstDef ConstExp
%type <init_val>  InitVal Initval ConstInitVal Constinitval

%start CompUnit
%%
    CompUnit: Compunit
            {
                auto comp_unit = new CompUnitAST;
                comp_unit->comp_unit = $1;
                comp_unit->position.line = cur_pos.line;
                comp_unit->position.col = cur_pos.col;
                ast = move(comp_unit);
            }
    Compunit: Compunit Decl
            {
                auto ast = reinterpret_cast<CompunitAST*>($1);
                ast->decl_list.push_back($2);
                root = ast;
                $$ = ast;
                pos($$);
            }
            | Compunit FuncDef
            {
                auto ast = reinterpret_cast<CompunitAST*>($1);
                ast->decl_list.push_back($2);
                root = ast;
                $$ = ast;
                pos($$);
            }
            | Decl
            {
                auto ast = new CompunitAST();
                ast->decl_list.push_back($1);
                root = ast;
                $$ = ast;
                pos($$);
            }
            | FuncDef
            {
                auto ast = new CompunitAST();
                ast->decl_list.push_back($1);
                root = ast;
                $$ = ast;
                pos($$);
            }
            ;
        Decl: ConstDecl
            {
                auto ast = new DeclAST();
                ast->tp = AstType::ConstDecl;
                ast->const_or_var_decl = $1;
                $$ = ast;
                pos($$);
            }
            | VarDecl
            {
                auto ast = new DeclAST();
                ast->tp = AstType::VarDecl;
                ast->const_or_var_decl = $1;
                $$ = ast;
                pos($$);
            }
            ;
   ConstDecl: Constdecl ';'
            {
                $$ = $1;
                pos($$);
            }
            ;
    Constdecl: Y_CONST BType ConstDef
            {
                auto ast = new ConstDeclAST();
                ast->BType = $2;
                ast->ConstDefVec.push_back($3);
                $$ = ast;
                pos($$);
            }
            | Constdecl ',' ConstDef
            {
                reinterpret_cast<ConstDeclAST*>($1)->ConstDefVec.push_back($3);
                $$ = $1;
                pos($$);
            }
            ;

       BType: Y_INT 
            {
                $$ = VarType::INT;
            }
            | Y_VOID
            {
                $$ = VarType::VOID;
            }

    ConstDef: Constdef '=' ConstInitVal
            {
                auto ast = reinterpret_cast<VarDefAST*>($1);
                ast->InitValue = $3;
                $$ = ast;
                pos($$);
            }
            ;

    Constdef: _identifier 
            {
                auto ast = new VarDefAST();
                ast->VarIdent = $1;
                ast->InitValue = NULL;
                $$ = ast;
                pos($$);
            }
            | Constdef '[' ConstExp ']'
            {
                auto ast = reinterpret_cast<VarDefAST*>($1);
                ast->DimSizeVec.push_back($3);
                $$ = ast;
                pos($$);
            }
            ;
ConstInitVal: ConstExp
            {
                $$ = new InitValTree<BaseAST*>();
                $$->keys.push_back($1); 
            }
            | '{' '}'
            {
                $$ = new InitValTree<BaseAST*>(); 
            }
            | '{' Constinitval '}'{
                $$ = new InitValTree<BaseAST*>();
                $$->childs.push_back($2);
            }
            ;
Constinitval: ConstInitVal 
            {
                $$ = $1;
            }
            | Constinitval ',' ConstInitVal 
            {
                $1->childs.push_back($3);
                $$ = $1;
            }
            ;
     VarDecl: Vardecl ';' 
            {
                $$ = $1;
                pos($$);
            }
            ;
     Vardecl: BType VarDef 
            {
                auto ast = new VarDeclAST();
                ast->BType = $1;
                ast->VarDefVec.push_back($2);
                $$ = ast;
                pos($$);
            }
            | Vardecl ',' VarDef
            {
                auto ast = reinterpret_cast<VarDeclAST*>($1);
                ast->VarDefVec.push_back($3);
                $$ = ast;
                pos($$);
            }
            ;
      VarDef: Vardef
            {
                $$ = $1;
                pos($$);
            }
            | Vardef '=' InitVal
            {
                auto ast = reinterpret_cast<VarDefAST*>($1);
                 ast->InitValue = $3;
                ast->IsInited = true;
                $$ = $1;
                pos($$);
            }
            ;
      Vardef: _identifier
            {
                auto ast = new VarDefAST();
                ast->VarIdent = $1;
                ast->InitValue = NULL;
                ast->IsInited = false;
                $$ = ast;
                pos($$);
            }
            | Vardef '[' ConstExp ']'
            {
                reinterpret_cast<VarDefAST*>($1)->DimSizeVec.push_back($3);
                $$ = $1;
                pos($$);
            }
            ;

     InitVal: Exp 
            {
                $$ = new InitValTree<BaseAST*>();
                $$->keys.push_back($1);
                
            }
            | '{' '}'
            {
                $$ = new InitValTree<BaseAST*>();
            }
            | '{' Initval '}'
            {
                $$ = new InitValTree<BaseAST*>();
                $$->childs.push_back($2);
            }
            ;

     Initval: InitVal
            {
                $$ = $1;
            }
            | Initval ',' InitVal
            {
                $1->childs.push_back($3);
                $$ = $1;
            }
            ;   

     FuncDef: BType _identifier '(' ')' Block
            {
                auto ast = new FuncDefAST();
                ast->func_type = $1;
                ast->ident = $2;
                ast->func_fparams = nullptr;
                ast->block = $5;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | BType _identifier '(' FuncFParams ')' Block
            {
                auto ast = new FuncDefAST();
                ast->func_type = $1;
                ast->ident = $2;
                ast->func_fparams = $4;
                ast->block = $6;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;

 FuncFParams: FuncFParam
            {
                auto ast = new FuncFParamsAST();
                ast->func_fparam.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | FuncFParams ',' FuncFParam
            {
                auto ast = reinterpret_cast<FuncFParamsAST*>($1);
                ast->func_fparam.push_back($3);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
  FuncFParam: BType _identifier
            {
                auto ast = new FuncFParamAST();
                ast->tp = ArgsType::Int32;
                ast->Btype = $1;
                ast->ident = $2;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Funcfparam
            {
                auto ast = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
  Funcfparam: BType _identifier '[' ']'
            {
                auto ast = new FuncFParamAST();
                ast->tp = ArgsType::Int32Array;
                ast->Btype = $1;
                ast->ident = $2;
                if(ast->EmitHighestDimFlag==true){
                    LOG(ERROR)<<"Array Parameter "<<*$2<<"Emit Too Many Dimensions\n";
                    exit(-1);
                }
                ast->EmitHighestDimFlag = true;
                ast->dimension.push_back(0);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Funcfparam '[' Exp ']'
            {
                auto ast = reinterpret_cast<FuncFParamAST*>($1);
                int temp;
                if($3->GetConstVal(temp)){
                    LOG(ERROR)<<"Array Parameter "<<*reinterpret_cast<FuncFParamAST*>($1)->ident<<" Has Varieble Dimension\n";
                    exit(-1);
                }
                ast->dimension.push_back(temp);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;

       Block: '{' block '}'
            {
                auto ast = new BlockAST();
                ast->block = $2;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | '{' '}'
            {
                auto ast = new BlockAST();
                ast->block = nullptr;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
       block: BlockItem
            {
                auto ast = new blockAST();
                ast->block_item.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | block BlockItem
            {
                auto ast = reinterpret_cast<blockAST*>($1);
                ast->block_item.push_back($2);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
   BlockItem: Decl
            {
                auto ast = new BlockItemAST();
                ast->tp="Decl";
                ast->decl_or_stmt = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Stmt
            {
                auto ast = new BlockItemAST();
                ast->tp = "Stmt";
                ast->decl_or_stmt = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
        Stmt: LVal '=' Exp ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::Assign;
                auto assign = new AssignAST();
                assign->LVal = $1;
                assign->ValueExp = $3;
                ast->ret_exp = assign;
                $$ = ast;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
            }
            | Exp ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::Exp;
                ast->ret_exp = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::Empty;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Block
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::Block;
                ast->ret_exp = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            |  Y_IF '(' Cond ')'  Stmt
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::If;
                ast->cond_exp = $3;
                ast->stmt_if = $5;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Y_IF '(' Cond ')' Stmt Y_ELSE Stmt
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::IfElse;
                ast->cond_exp = $3;
                ast->stmt_if = $5;
                ast->stmt_else = $7;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Y_WHILE '(' Cond ')'  Stmt
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::While;
                ast->cond_exp = $3;
                ast->stmt_while = $5;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Y_BREAK ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::Break;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Y_CONTINUE ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::Continue;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Y_RETURN ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::ReturnVoid;
                ast->ret_exp = nullptr;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Y_RETURN Exp ';'
            {
                auto ast = new StmtAST();
                ast->tp = StmtType::ReturnExp;
                ast->ret_exp = $2;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
         Exp: LOrExp
            {
                auto ast = new ExpAST();
                ast->lor_exp = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }           
            ;
        Cond: LOrExp
            ;
        LVal: _identifier
            {
                auto ast = new LValAST();
                ast->VarIdent = $1;
                $$ = ast;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
            }
            | LVal '[' Exp ']'
            {
                auto ast = reinterpret_cast<LValAST*>($1);
                ast->IndexVec.push_back($3);
                $$ = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
            }
            ;
  PrimaryExp: '(' Exp ')'
            {
                auto ast = new PrimaryExpAST();
                ast->tp = PrimaryType::Exp;
                ast->exp = $2;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | LVal
            {
                auto ast = new PrimaryExpAST();
                ast->tp = PrimaryType::LVal;
                ast->lval = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | Number
            {
                auto ast = new PrimaryExpAST();
                ast->tp = PrimaryType::Num;
                ast->number = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
      Number: _const_val
            {
                auto ast = new NumberAST();
                ast->num = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
    UnaryExp: PrimaryExp
            {
                auto ast = new UnaryExpAST();
                ast->tp = ExpType::Primary;
                ast->primary_exp = $1;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | _identifier '(' ')'
            {
                auto ast = new UnaryExpAST();
                ast->tp = ExpType::Call;
                ast->ident = $1;
                ast->func_rparam = nullptr;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | _identifier '(' FuncRParams ')'
            {
                auto ast = new UnaryExpAST();
                ast->tp = ExpType::Call;
                ast->ident = $1;
                ast->func_rparam = $3;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | UnaryOp UnaryExp
            {
                auto ast = new UnaryExpAST();
                ast->tp = ExpType::OpExp;
                ast->unary_op = $1;
                ast->unary_exp = $2;
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
     UnaryOp: '+'
            {
                auto ast = new UnaryOpAST();
                ast->op = "+";
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | '-'
            {
                auto ast = new UnaryOpAST();
                ast->op = "-";
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | '!'
            {
                auto ast = new UnaryOpAST();
                ast->op = "!";
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
 String: _string
        {
            auto ast = new StringAST();
            ast->StringLabel = *$1;
            SymbolTable::AddConstString(ast->StringLabel);
            delete $1;
            ast->position.line = cur_pos.line; 
            ast->position.col = cur_pos.col;
            $$ = ast;
        }
 FuncRParams: Exp
            {
                auto ast = new FuncRParamsAST();
                ast->exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            |
            String
            {
                auto ast = new FuncRParamsAST();
                ast->exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | FuncRParams ',' Exp
            {
                auto ast = reinterpret_cast<FuncRParamsAST*>($1);
                ast->exp.push_back($3);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
      MulExp: UnaryExp
            {
                auto ast = new MulExpAST();
                ast->unary_exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | MulExp '*' UnaryExp
            {
                auto ast = reinterpret_cast<MulExpAST*>($1);
                ast->unary_exp.push_back($3);
                ast->op.push_back("*");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | MulExp '/' UnaryExp
            {
                auto ast = reinterpret_cast<MulExpAST*>($1);
                ast->unary_exp.push_back($3);
                ast->op.push_back("/");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | MulExp '%' UnaryExp
            {
                auto ast = reinterpret_cast<MulExpAST*>($1);
                ast->unary_exp.push_back($3);
                ast->op.push_back("%");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
      AddExp: MulExp
            {
                auto ast = new AddExpAST();
                ast->mul_exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | AddExp '+' MulExp
            {
                auto ast = reinterpret_cast<AddExpAST*>($1);
                ast->mul_exp.push_back($3);
                ast->op.push_back("+");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | AddExp '-' MulExp
            {
                auto ast = reinterpret_cast<AddExpAST*>($1);
                ast->mul_exp.push_back($3);
                ast->op.push_back("-");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
      RelExp: AddExp
            {
                auto ast = new RelExpAST();
                ast->add_exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | RelExp _less AddExp
            {
                auto ast = reinterpret_cast<RelExpAST*>($1);
                ast->add_exp.push_back($3);
                ast->op.push_back("<");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | RelExp _greater AddExp
            {
                auto ast = reinterpret_cast<RelExpAST*>($1);
                ast->add_exp.push_back($3);
                ast->op.push_back(">");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | RelExp _less_equal AddExp
            {
                auto ast = reinterpret_cast<RelExpAST*>($1);
                ast->add_exp.push_back($3);
                ast->op.push_back("<=");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | RelExp _greater_equal AddExp
            {
                auto ast = reinterpret_cast<RelExpAST*>($1);
                ast->add_exp.push_back($3);
                ast->op.push_back(">=");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
       EqExp: RelExp
            {
                auto ast = new EqExpAST();
                ast->rel_exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | EqExp _equal RelExp
            {
                auto ast = reinterpret_cast<EqExpAST*>($1);
                ast->rel_exp.push_back($3);
                ast->op.push_back("==");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | EqExp _nequal RelExp
            {
                auto ast = reinterpret_cast<EqExpAST*>($1);
                ast->rel_exp.push_back($3);
                ast->op.push_back("!=");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
     LAndExp: EqExp
            {
                auto ast = new LAndExpAST();
                ast->eq_exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | LAndExp _logical_and EqExp
            {
                auto ast = reinterpret_cast<LAndExpAST*>($1);
                ast->eq_exp.push_back($3);
                ast->op.push_back("&&");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
      LOrExp: LAndExp
            {
                auto ast = new LOrExpAST();
                ast->land_exp.push_back($1);
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            | LOrExp _logical_or LAndExp
            {
                auto ast = reinterpret_cast<LOrExpAST*>($1);
                ast->land_exp.push_back($3);
                ast->op.push_back("||");
                ast->position.line = cur_pos.line; 
                ast->position.col = cur_pos.col;
                $$ = ast;
            }
            ;
    ConstExp: AddExp
            ;
      
%%

void yyerror(BaseAST* &ast, const char *s) {
  cerr << "error: line " <<ast->position.line <<" column: "<<ast->position.col<< endl;
}