#pragma once
#include "../CoreClass.hpp"
#include "../CFG.hpp"
// #include "../Type.hpp"
#include "BaseAst.hpp"

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

std::string ast_type_to_string(AST_Type type)
{
  switch (type)
  {
  case AST_INT:
    return "AST_INT";
  case AST_FLOAT:
    return "AST_FLOAT";
  case AST_VOID:
    return "AST_VOID";
  case AST_ADD:
    return "AST_ADD";
  case AST_SUB:
    return "AST_SUB";
  case AST_MUL:
    return "AST_MUL";
  case AST_MODULO:
    return "AST_MODULO";
  case AST_DIV:
    return "AST_DIV";
  case AST_GREAT:
    return "AST_GREAT";
  case AST_GREATEQ:
    return "AST_GREATEQ";
  case AST_LESS:
    return "AST_LESS";
  case AST_LESSEQ:
    return "AST_LESSEQ";
  case AST_EQ:
    return "AST_EQ";
  case AST_ASSIGN:
    return "AST_ASSIGN";
  case AST_NOTEQ:
    return "AST_NOTEQ";
  case AST_NOT:
    return "AST_NOT";
  case AST_AND:
    return "AST_AND";
  case AST_OR:
    return "AST_OR";
  default:
    return "UNKNOWN";
  }
}

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

  void print(int x) final
  {
    for (int i = 0; i < x; i++)
      std::cout << "  ";                                                      // 缩进
    std::cout << ast_type_to_string(OpList.front()) << " Level Expression\n"; // 转换函数

    for (auto &i : DataList)
    {
      i->print(x + !oplist.empty());
    }
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
};

using UnaryExp = BaseExp<HasOperand>; // 基本
using AddExp = BaseExp<MulExp>;       // 加减
using MulExp = BaseExp<UnaryExp>;     // 乘除
using RelExp = BaseExp<AddExp>;       // 逻辑
using EqExp = BaseExp<RelExp>;        //==
using LAndExp = BaseExp<EqExp>;       //&&
using LOrExp = BaseExp<LAndExp>;      //||

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

template <>
inline void BaseExp<LAndExp>::GetOperand(BasicBlock *block, BasicBlock *is_true,
                                         BasicBlock *is_false)
{
  for (auto it = DataList.begin(); it != DataList.end(); ++it)
  {
    if (it != std::prev(DataList.end()))
    {
      auto nxt_building = block->GenerateNewBlock();
      (*it)->GetOperand(block, is_true, nxt_building);
      if (!nxt_building->GetValUseList().is_empty())
      {
        block = nxt_building;
      }
      else
      {
        nxt_building->EraseFromManager();
        return;
      }
    }
    else
    {
      (*it)->GetOperand(block, is_true, is_false);
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
  void print(int x);
};

class Exps : public InnerBaseExps
{
public:
  Exps(AddExp *_data) : InnerBaseExps(_data) {}
  Type *GenerateArrayTypeDescriptor()
  {
    auto base_type = NewTypeFromIRDataType(Singleton<IR_DataType>());

    for (auto it = DataList.rbegin(); it != DataList.rend(); ++it)
    {
      Value *operand = (*it)->GetOperand(nullptr);
      if (auto fuc = dynamic_cast<ConstIRInt *>(operand))
      {
        base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
      }
      else if (auto fuc = dynamic_cast<ConstIRFloat *>(operand))
      {
        base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
      }
      else
      {
        throw std::runtime_error("Unexpected operand type in GetDeclDescipter()");
      }
    }
    return base_type;
  }
  Type *GenerateArrayTypeDescriptor(Type *base_type)
  {
    for (auto i = DataList.rbegin(); i != DataList.rend(); i++)
    {
      auto con = (*i)->GetOperand(nullptr);
      if (auto fuc = dynamic_cast<ConstIRInt *>(con))
      {
        base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
      }
      else if (auto fuc = dynamic_cast<ConstIRFloat *>(con))
      {
        base_type = ArrayType::NewArrayTypeGet(fuc->GetVal(), base_type);
      }
      else
        throw std::runtime_error("Unexpected operand type in GetDeclDescipter()");
    }
    return base_type;
  }
  std::vector<Operand> GetVisitDescripter(BasicBlock *block)
  {
    std::vector<Operand> tmp;
    for (auto &i : DataList)
      tmp.push_back(i->GetOperand(block));
    return tmp;
  }
};
