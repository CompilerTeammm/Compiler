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

class BaseDef:public Stmt
{
    protected:
    std::string name;
    std::unique_ptr<Exps> array_descriptor; 
    std::unique_ptr<InitVal> initval;
    public:
    BaseDef(std::string _name,Exps* _ad=nullptr,InitVal* _initval=nullptr):name(_name),array_descriptor(_ad),initval(_initval){}
    //待优化
    BasicBlock* GetInst(GetInstState state) final{
      if (array_descriptor) {
          auto tmp = array_descriptor->GenerateArrayTypeDescriptor();
          auto alloca = state.cur_block->GenerateAlloca(tmp, name);
  
          if (initval) { 
              Operand init = initval->GetOperand(tmp, state.cur_block);
              std::vector<Operand> args;
              auto src=new Var(Var::Constant,tmp,"");
              src->add_use(init);
              args.push_back(alloca);
              args.push_back(src);
              args.push_back(ConstIRInt::GetNewConstant(tmp->GetSize()));
              args.push_back(ConstIRBoolean::GetNewConstant(false));
              state.cur_block->GenerateCallInst("llvm.memcpy.p0.p0.i32", args, 0);
              std::vector<int> temp;
              static_cast<Initializer*>(init)->Var2Store(state.cur_block, name, temp);
          }
      } else {
          auto ir_data_type = Singleton<IR_DataType>();       
          auto decl_type = NewTypeFromIRDataType(ir_data_type);
          bool isConstDecl = Singleton<IR_CONSTDECL_FLAG>().flag == 1;
          if (isConstDecl) {
              Operand var = (initval) ? initval->GetFirst(nullptr) : 
                  (ir_data_type == IR_Value_INT) ? ConstIRInt::GetNewConstant() :
                  (ir_data_type == IR_Value_Float) ? ConstIRFloat::GetNewConstant() : 
                  (assert(0), Operand());
  
              var = (ir_data_type == IR_Value_INT) ? ToInt(var, nullptr) : ToFloat(var, nullptr);
              Singleton<Module>().Register(name, var);
          } else { 
              auto alloca = state.cur_block->GenerateAlloca(decl_type, name);
              if (initval) {
                  state.cur_block->GenerateStoreInst(initval->GetFirst(state.cur_block), alloca);
              }
          }
      }
      return state.cur_block;
  }
  
  void codegen() {
    if (array_descriptor != nullptr) { 
        auto desc = array_descriptor->GenerateArrayTypeDescriptor();
        auto var = new Var(Var::GlobalVar, desc, name);
        if (initval != nullptr) {
            var->add_use(initval->GetOperand(desc, nullptr));
        }
    } else { 
        auto inner_data_type = Singleton<IR_DataType>();
        auto decl_type = NewTypeFromIRDataType(inner_data_type);
        auto var = new Var(Var::GlobalVar, decl_type, name);
        bool is_const_decl = Singleton<IR_CONSTDECL_FLAG>().flag == 1;

        if (is_const_decl) {
            Operand init_value;
            if (initval == nullptr) { 
                switch (inner_data_type) {
                    case IR_Value_INT:
                        init_value = ConstIRInt::GetNewConstant();
                        break;
                    case IR_Value_Float:
                        init_value = ConstIRFloat::GetNewConstant();
                        break;
                    default:
                        throw std::runtime_error("Unsupported data type in BaseDef::codegen");
                }
            } else {
                init_value = initval->GetFirst(nullptr);
            }

            if (inner_data_type == IR_Value_INT) {
                init_value = ToInt(init_value, nullptr);
            } else if (inner_data_type == IR_Value_Float) {
                init_value = ToFloat(init_value, nullptr);
            } else {
                throw std::runtime_error("Unsupported data type in BaseDef::codegen");
            }

            Singleton<Module>().Register(name, init_value);
        } else {
            if (initval != nullptr) {
                var->add_use(initval->GetFirst(nullptr));
            }
        }
    }
}
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

class Initializer : public Value, public std::vector<Operand>
{
public:
  Initializer(Type *_tp) : Value(_tp) {}
  void Var2Store(BasicBlock *block, const std::string &name,
                 std::vector<int> &gep_data)
  {
    auto module = Singleton<Module>().GetValueByName(name);
    auto base_gep = dynamic_cast<GepInst *>(block->GenerateGepInst(module));

    for (size_t i = 0; i < this->size(); i++)
    {
      auto &handle = (*this)[i];
      gep_data.push_back(i);
      if (auto inits = dynamic_cast<Initializer *>(handle))
      {
        inits->Var2Store(block, name, gep_data);
      }
      else
      {
        if (!handle->isConst())
        {
          auto gep = base_gep;
          for (int j : gep_data)
            gep->add_use(ConstIRInt::GetNewConstant(j));

          block->GenerateStoreInst(handle, gep);
          if (handle->GetType()->GetTypeEnum() == IR_Value_INT)
            handle = ConstIRInt::GetNewConstant();
          else
            handle = ConstIRFloat::GetNewConstant();
        }
      }
      gep_data.pop_back();
    }
  }

  Operand GetInitVal(std::vector<int> &idx, int dep = 0)
  {
    auto basetp = dynamic_cast<HasSubType *>(GetType())->GetBaseType();
    auto getZero = [&]() -> Operand
    {
      if (basetp == IntType::NewIntTypeGet())
      {
        return ConstIRInt::GetNewConstant();
      }
      else if (basetp == FloatType::NewFloatTypeGet())
      {
        return ConstIRFloat::GetNewConstant();
      }
      else
      {
        return ConstIRBoolean::GetNewConstant();
      }
    };
    int thissize = size();
    if (thissize == 0)
    {
      return getZero();
    }
    // 先检查索引是否越界
    auto arrType = dynamic_cast<ArrayType *>(type);
    if (!arrType)
    {
      return getZero(); // 如果tp不是数组，直接返回默认值
    }
    int limi = arrType->GetNum();
    auto i = idx[dep];
    if (i >= limi)
    {
      return getZero();
    }
    // 索引超出已初始化部分
    if (i >= thissize)
    {
      return getZero();
    }
    // 递归访问子数组
    auto handle = (*this)[i];
    if (auto inits = dynamic_cast<Initializer *>(handle))
    {
      return inits->GetInitVal(idx, dep + 1);
    }
    return handle;
  }

  void print();
};

class InitVal : public BaseAST
{
private:
  std::unique_ptr<BaseAST> val;

public:
  InitVal() = default;
  InitVal(BaseAST *_data) { val.reset(_data); }
  Operand GetFirst(BasicBlock *block)
  {
    if (auto fuc = dynamic_cast<AddExp *>(val.get()))
      return fuc->GetOperand(block);
    else
      assert(0);
  }
  Operand GetOperand(Type *_tp, BasicBlock *block)
  {
    if (auto fuc = dynamic_cast<AddExp *>(val.get()))
      return fuc->GetOperand(block);
    else if (auto fuc = dynamic_cast<InitVals *>(val.get()))
    {
      return fuc->GetOperand(_tp, block);
    }
    else
      return new Initializer(_tp);
  }
  void print(int x);
};

class InitVals : public BaseAST
{
  BaseList<InitVal> DataList;

public:
  InitVals(InitVal *_data) { DataList.push_back(_data); }
  Operand GetOperand(Type *_tp, BasicBlock *block)
  {
    assert(_tp->GetTypeEnum() == IR_ARRAY);

    auto ret = new Initializer(_tp);
    size_t offs = 0;
    std::map<Type *, Type *> type_map;
    for (ArrayType *atp = dynamic_cast<ArrayType *>(_tp); atp; atp = dynamic_cast<ArrayType *>(atp->GetSubType()))
    {
      type_map[atp->GetSubType()] = atp;
    }
    auto max_type = [&](Type *tp) -> Type *
    {
      while (offs % tp->GetSize() != 0)
      {
        tp = dynamic_cast<ArrayType *>(tp)->GetSubType();
      }
      return tp;
    };

    auto sub = dynamic_cast<ArrayType *>(_tp)->GetSubType();

    for (auto &i : DataList)
    {
      Type *expectedType = max_type(sub); // 缓存结果
      Operand tmp = i->GetOperand(expectedType, block);

      if (tmp->GetType() != expectedType)
      {
        if (Singleton<IR_DataType>() == IR_Value_INT)
          tmp = ToInt(tmp, block);
        else if (Singleton<IR_DataType>() == IR_Value_Float)
          tmp = ToFloat(tmp, block);
        else
          assert(0);
      }

      offs += tmp->GetType()->GetSize();
      ret->push_back(tmp);
      Type *lastType = ret->back()->GetType();
      while (lastType != expectedType)
      {
        auto upper = dynamic_cast<ArrayType *>(type_map[lastType]);
        int ele = upper->GetNum();
        auto omit = new Initializer(upper);

        for (int j = 0; j < ele; j++)
        {
          omit->push_back(ret->back());
          ret->pop_back();
        }
        std::reverse(omit->begin(), omit->end());
        ret->push_back(omit);
        lastType = ret->back()->GetType();
      }
    }
    Type *lastType = ret->back()->GetType();
    while (lastType != sub)
    {
      auto upper = dynamic_cast<ArrayType *>(type_map[lastType]);
      int ele = upper->GetNum();
      auto omit = new Initializer(upper);
      for (int i = 0; i < ele && !ret->empty() && ret->back()->GetType() == upper->GetSubType(); i++)
      {
        omit->push_back(ret->back());
        ret->pop_back();
      }
      std::reverse(omit->begin(), omit->end());
      ret->push_back(omit);
      lastType = ret->back()->GetType();
    }
    return ret;
  }

  void print(int x);
};

// codegen:IR  print:AST
class CompUnit : public BaseAST
{
private:
  BaseList<BaseAST> DataList;

public:
  CompUnit(BaseAST *_data) { DataList.push_back(_data); }
  void codegen()
  {
    for (auto &i : DataList)
      i->codegen();
  }
  void print(int x);
};


class VarDef:public BaseDef
{
    public:
    VarDef(std::string _name,Exps* _ad,InitVal* _initval):BaseDef(_name,_ad,_initval){}
};

class VarDefs:public Stmt
{
    BaseList<VarDef> DataList;
    public:
    VarDefs(VarDef* _vardef){DataList.push_back(_vardef);}
    BasicBlock* GetInst(GetInstState state)final{
      for(auto&i:DataList)
      state.cur_block=i->GetInst(state);
      return state.cur_block;
    }

    void codegen(){
      for(auto&i:DataList)
      i->codegen();
    }
    void print(int x);
};

void AstToType(AST_Type type)
{
    switch (type)
    {
    case AST_INT:
        Singleton<IR_DataType>()=IR_Value_INT;
        break;
    case AST_FLOAT:
        Singleton<IR_DataType>()=IR_Value_Float;
        break;
    case AST_VOID:
    default:
        std::cerr<<"void as variable is not allowed!\n";
        assert(0);
    }
}

class VarDecl:public Stmt
{
    private:
    AST_Type type;
    std::unique_ptr<VarDefs> vardefs;
    public:
    VarDecl(AST_Type _tp,VarDefs* _vardefs):type(_tp),vardefs(_vardefs){}
    BasicBlock* GetInst(GetInstState state)final{
      Singleton<IR_CONSTDECL_FLAG>().flag=0;
      AstToType(type);
      return vardefs->GetInst(state);
    }
    void codegen(){
      Singleton<IR_CONSTDECL_FLAG>().flag=0;
      AstToType(type);
      vardefs->codegen();
    }
    void print(int x);
};


class ConstDef:public BaseDef
{
    public:
    ConstDef(std::string _name,Exps* _ad=nullptr,InitVal* _initval=nullptr):BaseDef(_name,_ad,_initval){}
};

class ConstDefs:public Stmt
{
    BaseList<ConstDef> DataList;
    public:
    ConstDefs(ConstDef* _constdef){DataList.push_back(_constdef);}
    BasicBlock* GetInst(GetInstState state)final{
      for(auto&i:DataList)
      state.cur_block=i->GetInst(state);
      return state.cur_block;
    }

    void codegen(){
      for(auto &i:DataList)
      i->codegen();
    }
    void print(int x);
};

class ConstDecl:public Stmt
{
    private:
    AST_Type type;
    std::unique_ptr<ConstDefs> constdefs;
    public:
    ConstDecl(AST_Type _tp,ConstDefs* _constdefs):type(_tp),constdefs(_constdefs){}
    BasicBlock* GetInst(GetInstState state)final{
      Singleton<IR_CONSTDECL_FLAG>().flag=1;      
      AstToType(type);
      return constdefs->GetInst(state);
    }

    void codegen(){
      Singleton<IR_CONSTDECL_FLAG>().flag=1;      
      AstToType(type);
      constdefs->codegen();
    }
    void print(int x);
};

class CallParams:public InnerBaseExps
{
    public:
    CallParams(AddExp* _addexp):InnerBaseExps(_addexp){}
    std::vector<Operand> CallParams::GetParams(BasicBlock* block){
      std::vector<Operand> params;
      for(auto &i:DataList)
          params.push_back(i->GetOperand(block));
      return params;
  }
};