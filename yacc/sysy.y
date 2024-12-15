
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
#include "common/initvalT.h"
#include "common/position.h"
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

%token Y_INT Y_FLOAT Y_VOID Y_CONST Y_IF Y_ELSE Y_WHILE Y_BREAK Y_CONTINUE Y_RETURN
%token Y_EQ Y_NOTEQ Y_GREAT Y_LESS Y_GREATEQ Y_LESSEQ Y_AND Y_OR
%token <StrVal> Y_ID _string
%token <iVal> num_INT
%token <fVal> num_FLOAT
%type <TypeVal> Type
%type <AstVal> CompUnit CompUnitt FuncDef Block BlockItems BlockItem Stmt FuncParam Decl ConstDecl VarDecl VarDecll VarDeff VarDef LVal Exp UnaryExp PrimaryExp Number UnaryOp AddExp MulExp RelExp EqExp LAndExp LOrExp FuncParams  FuncParamm CallParams Cond String ConstDecll ConstDeff ConstDef ConstExp
%type <InitVal> InitVal InitVals ConstInitVal ConstInitVall

%start CompUnit
%%
CompUnit: CompUnitt
        {
            auto node = new CompUnitAST;
            node->comp_unit = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            ast = move(node);
        }
CompUnitt: CompUnitt Decl
        {
            auto node = reinterpret_cast<CompunitAST*>($1);
            node->DeclList.push_back($2);
            root = node;
            $$ = node;
            pos($$);
        }
        | CompUnitt FuncDef
        {
            auto node = reinterpret_cast<CompunitAST*>($1);
            node->DeclList.push_back($2);
            root = node;
            $$ = node;
            pos($$);
        }
        | Decl
        {
            auto node = new CompunitAST();
            node->DeclList.push_back($1);
            root = node;
            $$ = node;
            pos($$);
        }
        | FuncDef
        {
            auto node = new CompunitAST();
            node->DeclList.push_back($1);
            root = node;
            $$ = node;
            pos($$);
        }
        ;
Decl: ConstDecl
    {
        auto node = new DeclAST();
        node->tp = AstType::ConstDecl;
        node->const_or_var_decl = $1;
        $$ = node;
        pos($$);
    }
    | VarDecl
    {
        auto node = new DeclAST();
        node->tp = AstType::VarDecl;
        node->const_or_var_decl = $1;
        $$ = node;
        pos($$);
    }
    ;
ConstDecl: ConstDecll ';'
        {
            $$ = $1;
            pos($$);
        }
        ;
ConstDecll: Y_CONST Type ConstDef
        {
            auto node = new ConstDeclAST();
            node->Type = $2;
            node->ConstDefVec.push_back($3);
            $$ = node;
            pos($$);
        }
        | ConstDecll ',' ConstDef
        {
            reinterpret_cast<ConstDeclAST*>($1)->ConstDefVec.push_back($3);
            $$ = $1;
            pos($$);
        }
        ;

Type: Y_INT 
    {
        $$ = VarType::INT;
    }
    | Y_FLOAT
    {
        $$ = VarType::FLOAT;
    }
    | Y_VOID
    {
        $$ = VarType::VOID;
    }

ConstDef: ConstDeff '=' ConstInitVal
        {
            auto node = reinterpret_cast<VarDefAST*>($1);
            node->InitValue = $3;
            $$ = node;
            pos($$);
        }
        ;

ConstDeff: Y_ID 
        {
            auto node = new VarDefAST();
            node->VarIdent = $1;
            node->InitValue = NULL;
            $$ = node;
            pos($$);
        }
        | ConstDeff '[' ConstExp ']'
        {
            auto node = reinterpret_cast<VarDefAST*>($1);
            node->DimSizeVec.push_back($3);
            $$ = node;
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
        | '{' ConstInitVall '}'{
            $$ = new InitValTree<BaseAST*>();
            $$->childs.push_back($2);
        }
        ;
ConstInitVall: ConstInitVal 
            {
                $$ = $1;
            }
            | ConstInitVall ',' ConstInitVal 
            {
                $1->childs.push_back($3);
                $$ = $1;
            }
            ;
VarDecl: VarDecll ';' 
        {
            $$ = $1;
            pos($$);
        }
        ;
VarDecll: Type VarDef 
        {
            auto node = new VarDeclAST();
            node->Type = $1;
            node->VarDefVec.push_back($2);
            $$ = node;
            pos($$);
        }
        | VarDecll ',' VarDef
        {
            auto node = reinterpret_cast<VarDeclAST*>($1);
            node->VarDefVec.push_back($3);
            $$ = node;
            pos($$);
        }
        ;
VarDef: VarDeff
    {
        $$ = $1;
        pos($$);
    }
    | VarDeff '=' InitVal
    {
        auto node = reinterpret_cast<VarDefAST*>($1);
        node->InitValue = $3;
        node->IsInited = true;
        $$ = $1;
        pos($$);
    }
    ;
VarDeff: Y_ID
    {
        auto node = new VarDefAST();
        node->VarIdent = $1;
        node->InitValue = NULL;
        node->IsInited = false;
        $$ = node;
        pos($$);
    }
    | VarDeff '[' ConstExp ']'
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
    | '{' InitVals '}'
    {
        $$ = new InitValTree<BaseAST*>();
        $$->childs.push_back($2);
    }
    ;

InitVals: InitVal
        {
            $$ = $1;
        }
        | InitVals ',' InitVal
        {
            $1->childs.push_back($3);
            $$ = $1;
        }
        ;   

FuncDef: Type Y_ID '(' ')' Block
        {
            auto node = new FuncDefAST();
            node->func_type = $1;
            node->ident = $2;
            node->block = $5;
            node->func_fparams = nullptr;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | Type Y_ID '(' FuncParams ')' Block
        {
            auto node = new FuncDefAST();
            node->func_type = $1;
            node->ident = $2;
            node->func_fparams = $4;
            node->block = $6;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;

FuncParams: FuncParam
        {
            auto node = new FuncFParamsAST();
            node->func_fparam.push_back($1);
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | FuncParams ',' FuncParam
        {
            auto node = reinterpret_cast<FuncFParamsAST*>($1);
            node->func_fparam.push_back($3);
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;
FuncParam: Type Y_ID
        {
            auto node = new FuncFParamAST();

            // 根据 Type 的值设置 tp 和 Btype
            if ($1 == VarType::INT)  // 如果 Type 是 Int
            {
                node->tp = ArgsType::Int32;
                node->Btype = VarType::INT;
            }
            else if ($1 == VarType::FLOAT)  // 如果 Type 是 Float
            {
                node->tp = ArgsType::Float32;
                node->Btype = VarType::FLOAT;
            }

            node->ident = $2;
            node->position.line = current.line; 
            node->position.col = current.col;

            $$ = node;
        }
        | FuncParamm
        {
            auto node = $1;
            node->position.line = current.line; 
            node->position.col = current.col;

            $$ = node;
        }
        ;

FuncParamm: Type Y_ID '[' ']'
{
    auto node = new FuncFParamAST();
    if ($1 == VarType::INT) {
        node->tp = ArgsType::Int32Array;  // 如果是 Int32 类型，标记为数组
    } else if ($1 == VarType::FLOAT) {
        node->tp = ArgsType::Float32Array;  // 如果是 Float32 类型，标记为数组
    } else {
        exit(-1);  // 如果类型不支持，直接退出
    }

    node->Btype = $1;
    node->ident = $2;
    if (node->EmitHighestDimFlag == true) {
        exit(-1);  // 防止重复设置最高维度标志
    }
    node->EmitHighestDimFlag = true;
    node->dimension.push_back(0);  // 默认维度初始化为 0
    node->position.line = current.line;
    node->position.col = current.col;
    $$ = node;  // 返回解析后的节点
}

| FuncParamm '[' Exp ']'
{
    auto node = reinterpret_cast<FuncFParamAST*>($1);
    int temp;  // 用于存储常量值

    // 获取维度的常量值
    if (!$3->GetConstVal(temp)) {
        node->dimension.push_back(temp);  // 如果维度是常量，加入维度列表
    } else {
        float temp_float;
        if (!$3->GetConstVal(temp_float)) {  // 如果无法转换为 int，尝试转换为 float
            exit(-1);  // 如果维度无法转换为常量值，退出
        }
        node->dimension.push_back(static_cast<int>(temp_float));  // 转换为 int 并加入维度列表
    }

    node->position.line = current.line;
    node->position.col = current.col;
    $$ = node;  // 返回解析后的节点
}
;

Block: '{' BlockItems '}'
    {
        auto node = new BlockAST();
        node->block = $2;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | '{' '}'
    {
        auto node = new BlockAST();
        node->block = nullptr;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
BlockItems: BlockItem
    {
        auto node = new blockAST();
        node->block_item.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | BlockItems BlockItem
    {
        auto node = reinterpret_cast<blockAST*>($1);
        node->block_item.push_back($2);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
BlockItem: Decl
        {
            auto node = new BlockItemAST();
            node->tp="Decl";
            node->decl_or_stmt = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | Stmt
        {
            auto node = new BlockItemAST();
            node->tp = "Stmt";
            node->decl_or_stmt = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;
Stmt: LVal '=' Exp ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::Assign;
        auto assign = new AssignAST();
        assign->LVal = $1;
        assign->ValueExp = $3;
        node->ret_exp = assign;
        $$ = node;
        node->position.line = current.line; 
        node->position.col = current.col;
    }
    | Exp ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::Exp;
        node->ret_exp = $1;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::Empty;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Block
    {
        auto node = new StmtAST();
        node->tp = StmtType::Block;
        node->ret_exp = $1;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    |  Y_IF '(' Cond ')'  Stmt
    {
        auto node = new StmtAST();
        node->tp = StmtType::If;
        node->cond_exp = $3;
        node->stmt_if = $5;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Y_IF '(' Cond ')' Stmt Y_ELSE Stmt
    {
        auto node = new StmtAST();
        node->tp = StmtType::IfElse;
        node->cond_exp = $3;
        node->stmt_if = $5;
        node->stmt_else = $7;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Y_WHILE '(' Cond ')'  Stmt
    {
        auto node = new StmtAST();
        node->tp = StmtType::While;
        node->cond_exp = $3;
        node->stmt_while = $5;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Y_BREAK ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::Break;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Y_CONTINUE ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::Continue;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Y_RETURN ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::ReturnVoid;
        node->ret_exp = nullptr;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | Y_RETURN Exp ';'
    {
        auto node = new StmtAST();
        node->tp = StmtType::ReturnExp;
        node->ret_exp = $2;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
    Exp: LOrExp
    {
        auto node = new ExpAST();
        node->lor_exp = $1;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }           
    ;
Cond: LOrExp
    ;
LVal: Y_ID
    {
        auto node = new LValAST();
        node->VarIdent = $1;
        $$ = node;
        node->position.line = current.line; 
        node->position.col = current.col;
    }
    | LVal '[' Exp ']'
    {
        auto node = reinterpret_cast<LValAST*>($1);
        node->IndexVec.push_back($3);
        $$ = $1;
        node->position.line = current.line; 
        node->position.col = current.col;
    }
    ;
PrimaryExp: '(' Exp ')'
        {
            auto node = new PrimaryExpAST();
            node->tp = PrimaryType::Exp;
            node->exp = $2;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | LVal
        {
            auto node = new PrimaryExpAST();
            node->tp = PrimaryType::LVal;
            node->lval = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | Number
        {
            auto node = new PrimaryExpAST();
            NumberAST* numNode = dynamic_cast<NumberAST*>($1);
            if (numNode->numType == NumberAST::NumberType::Int) {
                node->tp = PrimaryType::NumInt;
            } else {
                node->tp = PrimaryType::NumFloat;
            }
            node->number = numNode;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;

Number: num_INT
        {
            auto node = new NumberAST();
            node->numType = NumberAST::NumberType::Int;
            node->intNum = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | num_FLOAT
        {
            auto node = new NumberAST();
            node->numType = NumberAST::NumberType::Float;
            node->floatNum = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;

UnaryExp: PrimaryExp
        {
            auto node = new UnaryExpAST();
            node->tp = ExpType::Primary;
            node->primary_exp = $1;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | Y_ID '(' ')'
        {
            auto node = new UnaryExpAST();
            node->tp = ExpType::Call;
            node->ident = $1;
            node->func_rparam = nullptr;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | Y_ID '(' CallParams ')'
        {
            auto node = new UnaryExpAST();
            node->tp = ExpType::Call;
            node->ident = $1;
            node->func_rparam = $3;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | UnaryOp UnaryExp
        {
            auto node = new UnaryExpAST();
            node->tp = ExpType::OpExp;
            node->unary_op = $1;
            node->unary_exp = $2;
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;
UnaryOp: '+'
    {
        auto node = new UnaryOpAST();
        node->op = "+";
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | '-'
    {
        auto node = new UnaryOpAST();
        node->op = "-";
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | '!'
    {
        auto node = new UnaryOpAST();
        node->op = "!";
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;

String: _string
    {
        auto node = new StringAST();
        node->StringLabel = *$1;
        SymbolTable::AddConstString(node->StringLabel);
        delete $1;
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
CallParams: Exp
        {
            auto node = new CallParamsAST();
            node->exp.push_back($1);
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        |
        String
        {
            auto node = new CallParamsAST();
            node->exp.push_back($1);
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        | CallParams ',' Exp
        {
            auto node = reinterpret_cast<CallParamsAST*>($1);
            node->exp.push_back($3);
            node->position.line = current.line; 
            node->position.col = current.col;
            $$ = node;
        }
        ;
MulExp: UnaryExp
    {
        auto node = new MulExpAST();
        node->unary_exp.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | MulExp '*' UnaryExp
    {
        auto node = reinterpret_cast<MulExpAST*>($1);
        node->unary_exp.push_back($3);
        node->op.push_back("*");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | MulExp '/' UnaryExp
    {
        auto node = reinterpret_cast<MulExpAST*>($1);
        node->unary_exp.push_back($3);
        node->op.push_back("/");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | MulExp '%' UnaryExp
    {
        auto node = reinterpret_cast<MulExpAST*>($1);
        node->unary_exp.push_back($3);
        node->op.push_back("%");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
AddExp: MulExp
    {
        auto node = new AddExpAST();
        node->mul_exp.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | AddExp '+' MulExp
    {
        auto node = reinterpret_cast<AddExpAST*>($1);
        node->mul_exp.push_back($3);
        node->op.push_back("+");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | AddExp '-' MulExp
    {
        auto node = reinterpret_cast<AddExpAST*>($1);
        node->mul_exp.push_back($3);
        node->op.push_back("-");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
RelExp: AddExp
    {
        auto node = new RelExpAST();
        node->add_exp.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | RelExp Y_LESS AddExp
    {
        auto node = reinterpret_cast<RelExpAST*>($1);
        node->add_exp.push_back($3);
        node->op.push_back("<");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | RelExp Y_GREAT AddExp
    {
        auto node = reinterpret_cast<RelExpAST*>($1);
        node->add_exp.push_back($3);
        node->op.push_back(">");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | RelExp Y_LESSEQ AddExp
    {
        auto node = reinterpret_cast<RelExpAST*>($1);
        node->add_exp.push_back($3);
        node->op.push_back("<=");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | RelExp Y_GREATEQ AddExp
    {
        auto node = reinterpret_cast<RelExpAST*>($1);
        node->add_exp.push_back($3);
        node->op.push_back(">=");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
EqExp: RelExp
    {
        auto node = new EqExpAST();
        node->rel_exp.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | EqExp Y_EQ RelExp
    {
        auto node = reinterpret_cast<EqExpAST*>($1);
        node->rel_exp.push_back($3);
        node->op.push_back("==");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | EqExp Y_NOTEQ RelExp
    {
        auto node = reinterpret_cast<EqExpAST*>($1);
        node->rel_exp.push_back($3);
        node->op.push_back("!=");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
LAndExp: EqExp
    {
        auto node = new LAndExpAST();
        node->eq_exp.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | LAndExp Y_AND EqExp
    {
        auto node = reinterpret_cast<LAndExpAST*>($1);
        node->eq_exp.push_back($3);
        node->op.push_back("&&");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
LOrExp: LAndExp
    {
        auto node = new LOrExpAST();
        node->land_exp.push_back($1);
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    | LOrExp Y_OR LAndExp
    {
        auto node = reinterpret_cast<LOrExpAST*>($1);
        node->land_exp.push_back($3);
        node->op.push_back("||");
        node->position.line = current.line; 
        node->position.col = current.col;
        $$ = node;
    }
    ;
ConstExp: AddExp
        ;
      
%%

void yyerror(BaseAST* &ast, const char *s) {
  cerr << "error: line " <<ast->position.line <<" column: "<<ast->position.col<< endl;
}