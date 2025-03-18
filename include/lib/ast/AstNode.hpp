// #pragma once
// // #include "../CoreClass.hpp"
// #include "../CoreClass.hpp"
// // #include "../Type.hpp"
// #include "BaseAst.hpp"
// #include "MagicEnum.hpp"

#pragma once
#include "../CFG.hpp"
#include "../MyList.hpp"
#include "../MagicEnum.hpp"
#include "BaseAst.hpp"
#include <cassert>
#include <list>
#include <memory>
#include <string>

class BaseAST;
class CompUnit; // codegen, acquire what's in list has codegen
class Grammar_Assistance;
class ConstDecl; // codegen(CompUnit) GetInst
class ConstDefs; // codegen(CompUnit) GetInst
class InnerBaseExps;
class InitVal;
class InitVals;
class VarDecl;
class VarDefs;
class FuncDef;
class FuncParams;
class FuncParam;
class Block;
class BlockItems;
class AssignStmt;
class ExpStmt;
class WhileStmt;
class IfStmt;
class BreakStmt;
class ContinueStmt;
class ReturnStmt;
class LVal;
template <typename T>
class BaseExp;
class FunctionCall;
template <typename T>
class ConstValue;
class BaseDef;

enum AST_Type
{
  AST_INT,
  AST_FLOAT,
  AST_VOID,
  AST_ADD,
  AST_SUB,
  AST_MUL,
  AST_MODULO,
  AST_DIV,
  AST_GREAT,
  AST_GREATEQ,
  AST_LESS,
  AST_LESSEQ,
  AST_EQ,
  AST_ASSIGN,
  AST_NOTEQ,
  AST_NOT,
  AST_AND,
  AST_OR,
};

void AstToType(AST_Type type);

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
  std::list<AST_Type> OpList; // 操作符
  BaseList<T> DataList;       // 数据

  BaseExp(T *_data)
  {
    DataList.push_back(_data);
  }
  void push_back(AST_Type _tp)
  {
    OpList.push_back(_tp);
  }
  void push_front(AST_Type _tp)
  {
    OpList.push_front(_tp);
  }
  void push_back(T *_data)
  {
    DataList.push_back(_data);
  }
  void push_front(T *_data)
  {
    DataList.push_front(_data);
  }

  // 短路求值逻辑
  void GetOperand(BasicBlock *block, BasicBlock *is_true, BasicBlock *is_false)
  {
    std::cerr << "Only prepare for short circuit\n";
    assert(0);
  }

  Operand GetOperand(BasicBlock *block) final
  {
    // Unary
    if constexpr (std::is_same_v<T, HasOperand>)
    {
      Operand ptr = DataList.front()->GetOperand(block);
      for (auto i = OpList.rbegin(); i != OpList.rend(); i++)
      {
        switch (*i)
        {
        case AST_NOT:
          switch (ptr->GetType()->GetTypeEnum())
          {
          case IR_Value_INT:
            if (dynamic_cast<BoolType *>(ptr->GetType()))
              ptr = BasicBlock::GenerateBinaryInst(block, ptr, BinaryInst::Op_E, ConstIRBoolean::GetNewConstant());
            else
              ptr = BasicBlock::GenerateBinaryInst(block, ptr, BinaryInst::Op_E, ConstIRInt::GetNewConstant());
            break;
          case IR_Value_Float:
            ptr = BasicBlock::GenerateBinaryInst(block, ConstIRFloat::GetNewConstant(), BinaryInst::Op_E, ptr);
            break;
          case IR_PTR:
            ptr = BasicBlock::GenerateBinaryInst(block, ConstPtr::GetNewConstant(ptr->GetType()), BinaryInst::Op_E, ptr);
            break;
          default:
            assert(0);
          }
        case AST_ADD:
          break;
        case AST_SUB:
          if (ptr->GetType()->GetTypeEnum() == IR_Value_INT)
            ptr = BasicBlock::GenerateBinaryInst(block, ConstIRInt::GetNewConstant(), BinaryInst::Op_Sub, ptr);
          else
            ptr = BasicBlock::GenerateBinaryInst(block, ConstIRFloat::GetNewConstant(), BinaryInst::Op_Sub, ptr);
          break;
        default:
          assert(0);
          std::cerr << "Wrong Operator in BinaryExp\n";
          break;
        }
      }
      return ptr;
    }
    else
    {
      // Binary
      auto i = DataList.begin();
      auto oper = (*i)->GetOperand(block);
      for (auto &j : OpList)
      {
        i++;
        auto another = (*i)->GetOperand(block);
        BinaryInst::Operation opcode;
        switch (j)
        {
        case AST_ADD:
          opcode = BinaryInst::Op_Add;
          break;
        case AST_SUB:
          opcode = BinaryInst::Op_Sub;
          break;
        case AST_DIV:
          opcode = BinaryInst::Op_Div;
          break;
        case AST_MUL:
          opcode = BinaryInst::Op_Mul;
          break;
        case AST_EQ:
          opcode = BinaryInst::Op_E;
          break;
        case AST_AND:
          opcode = BinaryInst::Op_And;
          break;
        case AST_GREAT:
          opcode = BinaryInst::Op_G;
          break;
        case AST_LESS:
          opcode = BinaryInst::Op_L;
          break;
        case AST_GREATEQ:
          opcode = BinaryInst::Op_GE;
          break;
        case AST_LESSEQ:
          opcode = BinaryInst::Op_LE;
          break;
        case AST_MODULO:
          opcode = BinaryInst::Op_Mod;
          break;
        case AST_NOTEQ:
          opcode = BinaryInst::Op_NE;
          break;
        case AST_OR:
          opcode = BinaryInst::Op_Or;
          break;
        default:
          std::cerr << "No such Opcode\n";
          assert(0);
        }
        oper = BasicBlock::GenerateBinaryInst(block, oper, opcode, another);
      }
      return oper;
    }
  }

  void print(int x) final
  {
    if (!OpList.empty())
    {
      for (int i = 0; i < x; i++)
        std::cout << "  ";
      std::cout << magic_enum::enum_name(OpList.front())
                << " Level Expression\n";
    }
    for (auto &i : DataList)
      i->print(x + !OpList.empty());
  }
};

using UnaryExp = BaseExp<HasOperand>; // 基本
using MulExp = BaseExp<UnaryExp>;     // 乘除
using AddExp = BaseExp<MulExp>;       // 加减
using RelExp = BaseExp<AddExp>;       // 逻辑

template <>
inline Operand BaseExp<RelExp>::GetOperand(BasicBlock *block)
{
  auto i = DataList.begin();
  auto oper = (*i)->GetOperand(block);

  if (OpList.empty() && oper->GetType() != BoolType::NewBoolTypeGet())
  {
    switch (oper->GetType()->GetTypeEnum())
    {
    case IR_PTR:
      oper = block->GenerateBinaryInst(oper, BinaryInst::Op_NE,
                                       ConstPtr::GetNewConstant(oper->GetType()));
      break;
    case IR_Value_INT:
      oper = block->GenerateBinaryInst(oper, BinaryInst::Op_NE,
                                       ConstIRInt::GetNewConstant());
      break;
    case IR_Value_Float:
      oper = block->GenerateBinaryInst(oper, BinaryInst::Op_NE,
                                       ConstIRFloat::GetNewConstant());
      break;
    default:
      throw std::runtime_error("Unexpected operand type in GetOperand");
    }
    return oper;
  }

  static const std::unordered_map<int, BinaryInst::Operation> op_map = {
      {AST_EQ, BinaryInst::Op_E},
      {AST_NOTEQ, BinaryInst::Op_NE}};

  for (auto &j : OpList)
  {
    i++;
    auto another = (*i)->GetOperand(block);

    auto it = op_map.find(j);
    if (it == op_map.end())
    {
      std::cerr << "Wrong Opcode for EqExp\n";
      assert(0);
    }

    oper = block->GenerateBinaryInst(oper, it->second, another);
  }

  return oper;
}

using EqExp = BaseExp<RelExp>; //==

template <>
inline void BaseExp<EqExp>::GetOperand(BasicBlock *block, BasicBlock *_true,
                                       BasicBlock *_false)
{
  for (auto &i : DataList)
  {
    auto tmp = i->GetOperand(block);

    if (tmp->IsBoolean())
    {
      auto Const = static_cast<ConstIRBoolean *>(tmp);
      if (Const->GetVal() == false)
      {
        block->GenerateUnCondInst(_false);
        return;
      }
      if (i != DataList.back())
        continue;
      block->GenerateUnCondInst(_true);
      return;
    }

    auto nxt_block = (i != DataList.back()) ? block->GenerateNewBlock() : _true;
    block->GenerateCondInst(tmp, nxt_block, _false);
    block = nxt_block;
  }
}

using LAndExp = BaseExp<EqExp>; //&&

template <>
inline void BaseExp<LAndExp>::GetOperand(BasicBlock *block, BasicBlock *is_true,
                                         BasicBlock *is_false)
{
  for (auto it = DataList.begin(); it != DataList.end(); ++it)
  {
    if (it != std::prev(DataList.end()))
    {
      auto nxt_block = block->GenerateNewBlock();
      (*it)->GetOperand(block, is_true, nxt_block);
      if (!nxt_block->GetValUseList().is_empty())
      {
        block = nxt_block;
      }
      else
      {
        nxt_block->EraseFromManager();
        return;
      }
    }
    else
    {
      (*it)->GetOperand(block, is_true, is_false);
    }
  }
}

using LOrExp = BaseExp<LAndExp>; //||

class InnerBaseExps : public BaseAST
{
protected:
  BaseList<AddExp> DataList;

public:
  InnerBaseExps(AddExp *_data) { push_front(_data); }
  void push_front(AddExp *_data) { DataList.push_front(_data); }
  void push_back(AddExp *_data) { DataList.push_back(_data); }
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    for (auto &i : DataList)
      i->print(x + 1);
  }
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
  explicit InitVal(BaseAST *_data);
  Operand GetFirst(BasicBlock *block);
  Operand GetOperand(Type *_tp, BasicBlock *block);
  void print(int x)
  {
    BaseAST::print(x);
    if (val == nullptr)
      std::cout << ":empty{}\n";
    else
      std::cout << '\n', val->print(x + 1);
  }
};

class InitVals : public BaseAST
{
private:
  BaseList<InitVal> DataList;

public:
  explicit InitVals(InitVal *_data);
  void push_back(InitVal *_data);
  Operand GetOperand(Type *_tp, BasicBlock *block);
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    for (auto &i : DataList)
      ((BaseAST *)i.get())->print(x + 1);
  }
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
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << ":" << name << '\n';
    if (array_descriptor != nullptr)
      array_descriptor->print(x + 1);
    if (initval != nullptr)
      initval->print(x + 1);
  }
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
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    for (auto &i : DataList)
      i->print(x + 1);
    std::cout << '\n';
  }
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
  void print(int x)
  {
    for (auto &i : DataList)
      i->print(x);
  }
};

class VarDecl : public Stmt
{
private:
  AST_Type type;
  std::unique_ptr<VarDefs> vardefs;

public:
  VarDecl(AST_Type _tp, VarDefs *_vardefs);
  BasicBlock *GetInst(GetInstState state) final;
  void codegen();
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << ":" << magic_enum::enum_name(type) << '\n';
    vardefs->print(x + 1);
  }
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
  void print(int x)
  {
    for (auto &i : DataList)
      i->print(x);
  }
};

class ConstDecl : public Stmt
{
private:
  AST_Type type;
  std::unique_ptr<ConstDefs> constdefs;

public:
  ConstDecl(AST_Type _tp, ConstDefs *_constdefs);
  BasicBlock *GetInst(GetInstState state) final;
  void codegen();
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << ":TYPE:" << magic_enum::enum_name(type) << '\n';
    constdefs->print(x + 1);
  }
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
  void print(int x) final
  {
    BaseAST::print(x);
    std::cout << ":" << data << '\n';
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
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << name;
    if (callparams != nullptr)
      callparams->print(x + 1);
  }
};

class FuncParam : public BaseAST
{
private:
  AST_Type tp;
  std::string name;
  bool empty_square;
  std::unique_ptr<Exps> array_subscripts;

public:
  FuncParam(AST_Type _tp, std::string _name, bool is_empty = false, Exps *ptr = nullptr);

  void GetVar(Function &tmp);
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << ":" << magic_enum::enum_name(tp);
    if (empty_square == 1)
      std::cout << "ptr";
    std::cout << name;
    if (array_subscripts != nullptr)
      array_subscripts->print(x + 1);
  }
};

class FuncParams : public BaseAST
{
private:
  BaseList<FuncParam> DataList;

public:
  FuncParams(FuncParam *ptr);
  void push_back(FuncParam *ptr);
  void GetVar(Function &tmp);
  void print(int x)
  {
    for (auto &i : DataList)
      i->print(x);
  }
};

class BlockItems : public Stmt
{
private:
  BaseList<Stmt> DataList;

public:
  BlockItems(Stmt *ptr);
  void push_back(Stmt *ptr);

  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    for (auto &i : DataList)
      i->print(x + 1);
  }
};

class Block : public Stmt
{
private:
  std::unique_ptr<BlockItems> items;

public:
  Block(BlockItems *ptr);

  BasicBlock *GetInst(GetInstState state) final;
  void print(int x) { items->print(x); }
};

class FuncDef : public BaseAST
{
private:
  AST_Type tp;
  std::string name;
  std::unique_ptr<FuncParams> params;
  std::unique_ptr<Block> func_body;

public:
  FuncDef(AST_Type _tp, std::string _name, FuncParams *_params, Block *_func_body);

  void codegen();
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << ":" << name << ":" << magic_enum::enum_name(tp) << '\n';
    if (params != nullptr)
      params->print(x + 1);
    func_body->print(x + 1);
  }
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
  void print(int x)
  {
    BaseAST::print(x);
    if (array_descriptor != nullptr)
      std::cout << ":with array descripters";
    std::cout << ":" << name << '\n';
    if (array_descriptor != nullptr)
      array_descriptor->print(x + 1);
  }
};

class AssignStmt : public Stmt
{
private:
  std::unique_ptr<LVal> lval;
  std::unique_ptr<AddExp> exp;

public:
  AssignStmt(LVal *m, AddExp *n);

  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    assert(lval != nullptr);
    lval->print(x + 1);
    exp->print(x + 1);
  }
};

class ExpStmt : public Stmt
{
private:
  std::unique_ptr<AddExp> exp;

public:
  ExpStmt(AddExp *ptr);

  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    if (exp == nullptr)
      BaseAST::print(x);
    else
      exp->print(x);
  }
};

class WhileStmt : public Stmt
{
private:
  std::unique_ptr<LOrExp> cond;
  std::unique_ptr<Stmt> stmt;

public:
  WhileStmt(LOrExp *p1, Stmt *p2);

  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    assert(cond != nullptr && stmt != nullptr);
    cond->print(x + 1);
    stmt->print(x + 1);
  }
};

class IfStmt : public Stmt
{
private:
  std::unique_ptr<LOrExp> cond;
  std::unique_ptr<Stmt> true_stmt, false_stmt;

public:
  IfStmt(LOrExp *p0, Stmt *p1, Stmt *p2 = nullptr);

  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    assert(true_stmt != nullptr);
    true_stmt->print(x + 1);
    if (false_stmt != nullptr)
      false_stmt->print(x + 1);
  }
};

class BreakStmt : public Stmt
{
public:
  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
  }
};

class ContinueStmt : public Stmt
{
public:
  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
  }
};

class ReturnStmt : public Stmt
{
private:
  std::unique_ptr<AddExp> ret_val;

public:
  ReturnStmt(AddExp *ptr = nullptr);
  BasicBlock *GetInst(GetInstState state) final;
  void print(int x)
  {
    BaseAST::print(x);
    std::cout << '\n';
    if (ret_val != nullptr)
      ret_val->print(x + 1);
  }
};
