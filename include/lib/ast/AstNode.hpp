
#pragma once
#include "../CFG.hpp"
#include "BaseAst.hpp"

enum ASTNodeType
{
    // 基础类型
    TypeInt,
    TypeFloat,
    TypeVoid,

    // 算术运算
    OpAdd,
    OpSub,
    OpMul,
    OpDiv,
    OpModulo,

    // 比较运算
    OpGreater,
    OpGreaterEq,
    OpLess,
    OpLessEq,
    OpEqual,
    OpNotEqual,

    // 逻辑运算
    OpNot,
    OpAnd,
    OpOr,

    // 赋值
    OpAssign,
};

void AstToType(ASTNodeType type);

struct GetInstState
{
  BasicBlock *cur_block;
  BasicBlock *break_block;
  BasicBlock *continue_block;
};

class HasOperand : public BaseAST
{
public:
  virtual Operand GetOperand(BasicBlock *block) = 0;
};

class Stmt : public BaseAST
{
public:
  virtual BasicBlock *GetInst(GetInstState) = 0;
};

template <typename T>
class BaseExp : public HasOperand
{
public:
  std::list<ASTNodeType> OpList; // 操作符
  BaseList<T> DataList;       // 数据

  BaseExp(T *_data){DataList.push_back(_data);}
  void push_back(ASTNodeType _tp){OpList.push_back(_tp);}
  void push_front(ASTNodeType _tp){OpList.push_front(_tp);}
  void push_back(T *_data){DataList.push_back(_data);}
  void push_front(T *_data){DataList.push_front(_data);}

  // 短路求值逻辑
  void GetOperand(BasicBlock *block, BasicBlock *is_true, BasicBlock *is_false){}

  Operand GetOperand(BasicBlock* block) final {
    // Unary helpers
    auto genUnaryNot = [&](Operand val) -> Operand {
        switch (val->GetType()->GetTypeEnum()) {
        case IR_Value_INT:
            if (dynamic_cast<BoolType*>(val->GetType()))
                return static_cast<Operand>(BasicBlock::GenerateBinaryInst(block, val, BinaryInst::Op_E, ConstIRBoolean::GetNewConstant()));
            else
                return static_cast<Operand>(BasicBlock::GenerateBinaryInst(block, val, BinaryInst::Op_E, ConstIRInt::GetNewConstant()));
        case IR_Value_Float:
            return static_cast<Operand>(BasicBlock::GenerateBinaryInst(block, ConstIRFloat::GetNewConstant(), BinaryInst::Op_E, val));
        case IR_PTR:
            return static_cast<Operand>(BasicBlock::GenerateBinaryInst(block, ConstPtr::GetNewConstant(val->GetType()), BinaryInst::Op_E, val));
        default:
            throw std::runtime_error("Unsupported type for OpNot");
        }
    };

    auto genUnarySub = [&](Operand val) -> Operand {
        return (val->GetType()->GetTypeEnum() == IR_Value_INT)
                   ? static_cast<Operand>(BasicBlock::GenerateBinaryInst(block, ConstIRInt::GetNewConstant(), BinaryInst::Op_Sub, val))
                   : static_cast<Operand>(BasicBlock::GenerateBinaryInst(block, ConstIRFloat::GetNewConstant(), BinaryInst::Op_Sub, val));
    };

    if constexpr (std::is_same_v<T, HasOperand>) {
        // Unary
        Operand result = DataList.front()->GetOperand(block);
        for (auto it = OpList.rbegin(); it != OpList.rend(); ++it) {
            switch (*it) {
            case OpNot: result = genUnaryNot(result); break;
            case OpAdd: break; // unary plus
            case OpSub: result = genUnarySub(result); break;
            default: throw std::runtime_error("Unsupported unary operator");
            }
        }
        return result;
    } else {
        // Binary
        auto it = DataList.begin();
        Operand result = (*it)->GetOperand(block);
        ++it;

        // static map, 不用 lambda
        static const std::unordered_map<ASTNodeType, BinaryInst::Operation> opMap = {
            {OpAdd, BinaryInst::Op_Add}, {OpSub, BinaryInst::Op_Sub},
            {OpMul, BinaryInst::Op_Mul}, {OpDiv, BinaryInst::Op_Div},
            {OpEqual, BinaryInst::Op_E}, {OpNotEqual, BinaryInst::Op_NE},
            {OpAnd, BinaryInst::Op_And}, {OpOr, BinaryInst::Op_Or},
            {OpGreater, BinaryInst::Op_G}, {OpLess, BinaryInst::Op_L},
            {OpGreaterEq, BinaryInst::Op_GE}, {OpLessEq, BinaryInst::Op_LE},
            {OpModulo, BinaryInst::Op_Mod}
        };

        for (auto& op : OpList) {
            Operand rhs = (*it)->GetOperand(block);
            ++it;

            auto mapIt = opMap.find(op);
            if (mapIt == opMap.end())
                throw std::runtime_error("Unsupported binary operator");

            result = BasicBlock::GenerateBinaryInst(block, result, mapIt->second, rhs);
        }
        return result;
    }
}
};

using UnaryExp = BaseExp<HasOperand>; // 基本
using MulExp = BaseExp<UnaryExp>;     // 乘除
using AddExp = BaseExp<MulExp>;       // 加减
using RelExp = BaseExp<AddExp>;       // 逻辑

template <>
inline Operand BaseExp<RelExp>::GetOperand(BasicBlock *block)
{
    auto it = DataList.begin();
    Operand result = (*it)->GetOperand(block);

    // 如果没有操作符且类型不是布尔，则转换为布尔值
    if (OpList.empty() && result->GetType() != BoolType::NewBoolTypeGet()) {
        Operand zeroValue = nullptr;
        switch (result->GetType()->GetTypeEnum()) {
        case IR_PTR:   zeroValue = ConstPtr::GetNewConstant(result->GetType()); break;
        case IR_Value_INT:   zeroValue = ConstIRInt::GetNewConstant(); break;
        case IR_Value_Float: zeroValue = ConstIRFloat::GetNewConstant(); break;
        default:    throw std::runtime_error("Unexpected operand type in GetOperand");
        }
        result = block->GenerateBinaryInst(result, BinaryInst::Op_NE, zeroValue);
        return result;
    }

    // 只处理 == 和 !=
    static const std::unordered_map<ASTNodeType, BinaryInst::Operation> opMap = {
        {OpEqual, BinaryInst::Op_E},
        {OpNotEqual, BinaryInst::Op_NE}
    };

    for (auto &op : OpList) {
        ++it;
        Operand rhs = (*it)->GetOperand(block);

        auto mapIt = opMap.find(op);
        if (mapIt == opMap.end())
            throw std::runtime_error("Unsupported operator in RelExp");

        result = block->GenerateBinaryInst(result, mapIt->second, rhs);
    }

    return result;
}

using EqExp = BaseExp<RelExp>; //==

template <>
inline void BaseExp<EqExp>::GetOperand(BasicBlock *block, BasicBlock *TrueBlock,
                                                   BasicBlock *FalseBlock)
{
    auto lastIt = std::prev(DataList.end());

    for (auto it = DataList.begin(); it != DataList.end(); ++it) {
        Operand tmp = (*it)->GetOperand(block);

        // 常量布尔值处理
        if (tmp->IsBoolean()) {
            auto constBool = static_cast<ConstIRBoolean*>(tmp);
            if (!constBool->GetVal()) {
                block->GenerateUnCondInst(FalseBlock);
                return;
            }
            if (it == lastIt) {
                block->GenerateUnCondInst(TrueBlock);
                return;
            }
            continue; // true 常量继续循环
        }

        // 非常量布尔值
        BasicBlock* nextBlock = (it != lastIt) ? block->GenerateNewBlock() : TrueBlock;
        block->GenerateCondInst(tmp, nextBlock, FalseBlock);
        block = nextBlock;
    }
}

using LAndExp = BaseExp<EqExp>; //&&

template <>
inline void BaseExp<LAndExp>::GetOperand(BasicBlock *block, BasicBlock *TrueBlock,
                                         BasicBlock *FalseBlock)
{
    auto endOperand = std::prev(DataList.end());

    for (auto nodeIt = DataList.begin(); nodeIt != DataList.end(); ++nodeIt)
    {
        if (nodeIt != endOperand)
        {
            // 构建中间块处理短路逻辑
            BasicBlock *nextblock = block->GenerateNewBlock();
            (*nodeIt)->GetOperand(block, TrueBlock, nextblock);

            if (nextblock->GetValUseList().is_empty())
            {
                // 如果中间块没有使用，直接删除
                nextblock->EraseFromManager();
                return;
            }
            block = nextblock; 
        }
        else
        {
            // 最终操作数跳转到真/假块
            (*nodeIt)->GetOperand(block, TrueBlock, FalseBlock);
        }
    }
}

using LOrExp = BaseExp<LAndExp>; //||
template <>
inline void BaseExp<LOrExp>::GetOperand(BasicBlock *block, BasicBlock *TrueBlock,
                                                   BasicBlock *FalseBlock) {
    auto curBlk = block;

    for (auto it = DataList.begin(); it != DataList.end(); ++it) {
        bool isLast = (std::next(it) == DataList.end());
        BasicBlock *NextBlock = isLast ? FalseBlock : curBlk->GenerateNewBlock();

        // 遇到真值立即跳转到 blkTrue，短路
        (*it)->GetOperand(curBlk, TrueBlock, NextBlock);

        if (!isLast) {
            // 如果中间块还有使用，就继续，否则直接删除
            if (!NextBlock->GetValUseList().is_empty()) {
                curBlk = NextBlock;
            } else {
                NextBlock->EraseFromManager();
                return;
            }
        }
    }
}

class InnerBaseExps : public BaseAST
{
protected:
  BaseList<AddExp> DataList;

public:
  InnerBaseExps(AddExp *_data) { push_front(_data); }
  void push_front(AddExp *_data) { DataList.push_front(_data); }
  void push_back(AddExp *_data) { DataList.push_back(_data); }
};

class Exps : public InnerBaseExps
{
public:
  Exps(AddExp *_data);
  Type *GenerateArrayTypeDescriptor();
  Type *GenerateArrayTypeDescriptor(Type *base_type);
  std::vector<Operand> GetVisitDescripter(BasicBlock *block);
};

class CallParams : public InnerBaseExps
{
public:
  CallParams(AddExp *_addexp);
  std::vector<Operand> GetParams(BasicBlock *block);
};

class InitVal : public BaseAST
{
private:
  std::unique_ptr<BaseAST> val;

public:
  InitVal() = default;
  InitVal(BaseAST *_data);
  Operand GetFirst(BasicBlock *block);
  Operand GetOperand(Type *_tp, BasicBlock *block);
};

class InitVals : public BaseAST
{
private:
  BaseList<InitVal> DataList;

public:
  explicit InitVals(InitVal *_data);
  void push_back(InitVal *_data);
  Operand GetOperand(Type *_tp, BasicBlock *block);
};

class BaseDef : public Stmt
{
protected:
  std::string name;
  std::unique_ptr<Exps> array_descriptor;
  std::unique_ptr<InitVal> initval;

public:
  BaseDef(std::string _name, Exps *_ad = nullptr, InitVal *_initval = nullptr);

  BasicBlock *GetInst(GetInstState) final;

  void codegen();
};

// codegen:IR  print:AST
class CompUnit : public BaseAST
{
private:
  BaseList<BaseAST> DataList;

public:
  explicit CompUnit(BaseAST *_data);
  void push_back(BaseAST *_data);
  void push_front(BaseAST *_data);
  void codegen();
};

class VarDef : public BaseDef
{
public:
  VarDef(std::string _name, Exps *_ad = nullptr, InitVal *_initval = nullptr) : BaseDef(_name, _ad, _initval) {}
};

class VarDefs : public Stmt
{
private:
  BaseList<VarDef> DataList;

public:
  explicit VarDefs(VarDef *_vardef);
  void push_back(VarDef *_data);
  BasicBlock *GetInst(GetInstState state) final;
  void codegen();
};

class VarDecl : public Stmt
{
private:
  ASTNodeType type;
  std::unique_ptr<VarDefs> vardefs;

public:
  VarDecl(ASTNodeType _tp, VarDefs *_vardefs);
  BasicBlock *GetInst(GetInstState state) final;
  void codegen();
};

class ConstDef : public BaseDef
{
public:
  ConstDef(std::string _name, Exps *_ad = nullptr, InitVal *_initval = nullptr) : BaseDef(_name, _ad, _initval) {}
};

class ConstDefs : public Stmt
{
private:
  BaseList<ConstDef> DataList;

public:
  ConstDefs(ConstDef *_constdef);
  void push_back(ConstDef *_data);
  BasicBlock *GetInst(GetInstState state) final;
  void codegen();
};

class ConstDecl : public Stmt
{
private:
  ASTNodeType type;
  std::unique_ptr<ConstDefs> constdefs;

public:
  ConstDecl(ASTNodeType _tp, ConstDefs *_constdefs);
  BasicBlock *GetInst(GetInstState state) final;
  void codegen();
};

template <typename T>
class ConstValue : public HasOperand
{
  T data;

public:
  ConstValue(T _data) : data(_data) {}
  Operand GetOperand(BasicBlock *block)
  {
    if (std::is_same<T, int>::value)
      return ConstIRInt::GetNewConstant(data);
    else
      return ConstIRFloat::GetNewConstant(data);
  }
};

class FunctionCall : public HasOperand
{
private:
  std::string name;
  std::unique_ptr<CallParams> callparams;

public:
  FunctionCall(std::string _name, CallParams *ptr = nullptr);
  Operand GetOperand(BasicBlock *block);
};

class FuncParam : public BaseAST
{
private:
  ASTNodeType tp;
  std::string name;
  bool empty_square;
  std::unique_ptr<Exps> array_subscripts;

public:
  FuncParam(ASTNodeType _tp, std::string _name, bool is_empty = false, Exps *ptr = nullptr);

  void GetVar(Function &tmp);
};

class FuncParams : public BaseAST
{
private:
  BaseList<FuncParam> DataList;

public:
  FuncParams(FuncParam *ptr);
  void push_back(FuncParam *ptr);
  void GetVar(Function &tmp);
};

class BlockItems : public Stmt
{
private:
  BaseList<Stmt> DataList;

public:
  BlockItems(Stmt *ptr);
  void push_back(Stmt *ptr);

  BasicBlock *GetInst(GetInstState state) final;
};

class Block : public Stmt
{
private:
  std::unique_ptr<BlockItems> items;

public:
  Block(BlockItems *ptr);

  BasicBlock *GetInst(GetInstState state) final;
};

class FuncDef : public BaseAST
{
private:
  ASTNodeType tp;
  std::string name;
  std::unique_ptr<FuncParams> params;
  std::unique_ptr<Block> func_body;

public:
  FuncDef(ASTNodeType _tp, std::string _name, FuncParams *_params, Block *_func_body);

  void codegen();
};

class LVal : public HasOperand
{
private:
  std::string name;
  std::unique_ptr<Exps> array_descriptor;

public:
  LVal(std::string _name, Exps *ptr = nullptr);
  std::string GetName();

  Operand GetPointer(BasicBlock *block);
  Operand GetOperand(BasicBlock *block);
};

class AssignStmt : public Stmt
{
private:
  std::unique_ptr<LVal> lval;
  std::unique_ptr<AddExp> exp;

public:
  AssignStmt(LVal *m, AddExp *n);

  BasicBlock *GetInst(GetInstState state) final;
};

class ExpStmt : public Stmt
{
private:
  std::unique_ptr<AddExp> exp;

public:
  ExpStmt(AddExp *ptr);

  BasicBlock *GetInst(GetInstState state) final;
};

class WhileStmt : public Stmt
{
private:
  std::unique_ptr<LOrExp> cond;
  std::unique_ptr<Stmt> stmt;

public:
  WhileStmt(LOrExp *p1, Stmt *p2);

  BasicBlock *GetInst(GetInstState state) final;

};

class IfStmt : public Stmt
{
private:
  std::unique_ptr<LOrExp> cond;
  std::unique_ptr<Stmt> true_stmt, false_stmt;

public:
  IfStmt(LOrExp *p0, Stmt *p1, Stmt *p2 = nullptr);

  BasicBlock *GetInst(GetInstState state) final;
};

class BreakStmt : public Stmt
{
public:
  BasicBlock *GetInst(GetInstState state) final;
};

class ContinueStmt : public Stmt
{
public:
  BasicBlock *GetInst(GetInstState state) final;
};

class ReturnStmt : public Stmt
{
private:
  std::unique_ptr<AddExp> ret_val;

public:
  ReturnStmt(AddExp *ptr = nullptr);
  BasicBlock *GetInst(GetInstState state) final;
};
