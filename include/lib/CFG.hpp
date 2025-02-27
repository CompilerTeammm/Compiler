#include "CoreClass.hpp"
#include "Type.hpp"
// #include <cassert>
#include <sstream>

// 这边的类都还暂未实现，先放，占位

// 所有常量数据的基类
class ConstantData : public Value
{
public:
  virtual ConstantData *clone(std::unordered_map<Operand, Operand> &mapping) override
  {
    return this;
  };
  ConstantData() = delete;
  ConstantData(Type *_tp) : Value(_tp) {}
  static ConstantData *getNullValue(Type *tp)
  {
    IR_DataType type = tp->GetTypeEnum();
    if (type == IR_DataType::IR_Value_INT)
      return ConstIRInt::GetNewConstant(0);
    else if (type == IR_DataType::IR_Value_Float)
      return ConstIRFloat::GetNewConstant(0);
    else
      return nullptr;
  }
  bool isConst() final { return true; }
  bool isZero()
  {
    if (auto Int = dynamic_cast<ConstIRInt *>(this))
      return Int->GetVal() == 0;
    else if (auto Float = dynamic_cast<ConstIRFloat *>(this))
      return Float->GetVal() == 0;
    else if (auto Bool = dynamic_cast<ConstIRBoolean *>(this))
      return Bool->GetVal() == false;
    else
      return false;
  }
};

class ConstIRBoolean : public ConstantData
{
  bool val;
  ConstIRBoolean(bool _val)
      : ConstantData(BoolType::NewBoolTypeGet()), val(_val)
  {
    if (val)
      name = "true";
    else
      name = "false";
  };

public:
  static ConstIRBoolean *GetNewConstant(bool _val = false)
  {
    static ConstIRBoolean true_const(true);
    static ConstIRBoolean false_const(false);
    if (_val)
      return &true_const;
    else
      return &false_const;
  }
  bool GetVal() { return val; }
};

class ConstIRInt : public ConstantData
{
  int val;
  ConstIRInt(int _val)
      : ConstantData(IntType::NewIntTypeGet()), val(_val)
  {
    name = std::to_string(val);
  };

public:
  static ConstIRInt *GetNewConstant(int _val = 0)
  {
    static std::map<int, ConstIRInt *> int_const_map;
    if (int_const_map.find(_val) == int_const_map.end())
      int_const_map[_val] = new ConstIRInt(_val);
    return int_const_map[_val];
  }
  int GetVal() { return val; }
};

class ConstIRFloat : public ConstantData
{
  float val;
  ConstIRFloat(float _val)
      : ConstantData(FloatType::NewFloatTypeGet()), val(_val)
  {
    // double更精确处理浮点数二进制
    double tmp = val;
    std::stringstream hexStream;
    // 十六进制
    hexStream << "0x" << std::hex << *(reinterpret_cast<long long *>(&tmp));
    name = hexStream.str();
  };

public:
  static ConstIRFloat *GetNewConstant(float _val = 0)
  {
    static std::map<float, ConstIRFloat *> float_const_map;
    if (float_const_map.find(_val) == float_const_map.end())
      float_const_map[_val] = new ConstIRFloat(_val);
    return float_const_map[_val];
  }
  float GetVal() { return val; }
  double GetValAsDouble() const { return static_cast<double>(val); }
};

class ConstPtr : public ConstantData
{
  ConstPtr(Type *_tp) : ConstantData(_tp) {}

public:
  static ConstPtr *GetNewConstant(Type *_tp)
  {
    static ConstPtr *const_ptr = new ConstPtr(_tp);
    return const_ptr;
  }
};

// 所有Inst
// 通用的clone,print方法暂时都还没有写
class LoadInst : public Instruction
{
public:
  LoadInst(Type *_tp) : Instruction(_tp, Op::Load) {};
  // 处理指针载入
  LoadInst(Operand _src) : Instruction(dynamic_cast<PointerType *>(_src->GetType())->GetSubType(), Op::Load)
  {
    assert(GetTypeEnum() == IR_Value_INT || GetTypeEnum() == IR_Value_Float ||
           GetTypeEnum() == IR_PTR);
    add_use(_src);
  }
  LoadInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;
};

class StoreInst : public Instruction
{
public:
  bool isUsed = false;
  StoreInst(Type *_tp) : Instruction(_tp, Op::Store) {};
  StoreInst(Operand _A, Operand _B)
  {
    add_use(_A);
    add_use(_B);
    name = "StoreInst";
    id = Op::Store;
  }
  Operand GetDef() final { return nullptr; }

  StoreInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;
};

class AllocaInst : public Instruction
{
public:
  bool isUsed()
  {
    auto &ValUseList = GetValUseList();
    if (ValUseList.is_empty())
      return false;
    return true;
  }
  bool AllZero = false;
  bool HasStored = false;
  AllocaInst(Type *_tp) : Instruction(_tp, Op::Alloca) {};
  // AllocaInst(std::string str, Type *_tp){}

  AllocaInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class CallInst : public Instruction
{
public:
  CallInst(Type *_tp) : Instruction(_tp, Op::Call) {};
  CallInst(Value *_func, std::vector<Operand> &_args, std::string label = "")
      : Instruction(_func->GetType(), Op::Call)
  {
    name += label;
    add_use(_func);
    for (auto &n : _args)
      add_use(n);
  }

  CallInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class RetInst : public Instruction
{
public:
  RetInst() { id = Op::Ret; }
  RetInst(Type *_tp) : Instruction(_tp, Op::Ret) {};
  RetInst(Operand op)
  {
    add_use(op);
    id = Op::Ret;
  }
  Operand GetDef() final { return nullptr; }

  RetInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class CondInst : public Instruction
{
public:
  CondInst(Type *_tp) : Instruction(_tp, Op::Cond) {};
  CondInst(Operand _cond, BasicBlock *_true, BasicBlock *_false)
  {
    add_use(_cond);
    add_use(_true);
    add_use(_false);
    id = Op::Cond;
  }
  Operand GetDef() final { return nullptr; }

  CondInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class UnCondInst : public Instruction
{
public:
  UnCondInst(Type *_tp) : Instruction(_tp, Op::UnCond) {};
  UnCondInst(BasicBlock *_BB)
  {
    add_use(_BB);
    id = Op::UnCond;
  }
  Operand GetDef() final { return nullptr; }

  UnCondInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class BinaryInst : public Instruction
{
public:
  enum Operation
  {
    Op_Add,
    Op_Sub,
    Op_Mul,
    Op_Div,
    Op_Mod,
    Op_And,
    Op_Or,
    Op_Xor,
    // cmp
    Op_E,
    Op_NE,
    Op_GE,
    Op_L,
    Op_LE,
    Op_G
  };

private:
  Operation op;
  bool Atomic = false;

public:
  BinaryInst(Type *_tp) : Instruction(_tp, Op::BinaryUnknown) {};
  BinaryInst(Operand _A, Operation _op, Operand _B, bool Atom = false);
};

bool isBinaryBool(BinaryInst::Operation op)
{
  switch (op)
  {
  case BinaryInst::Op_E:
  case BinaryInst::Op_NE:
  case BinaryInst::Op_G:
  case BinaryInst::Op_GE:
  case BinaryInst::Op_L:
  case BinaryInst::Op_LE:
    return true;
  default:
    return false;
  }
}

class ZextInst : public Instruction
{
public:
  ZextInst(Type *_tp) : Instruction(_tp, Op::Zext) {};
  ZextInst(Operand ptr) : Instruction(IntType::NewIntTypeGet(), Op::Zext)
  {
    add_use(ptr);
  }

  ZextInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class SextInst : public Instruction
{
public:
  SextInst(Type *_tp) : Instruction(_tp, Op::Sext) {};
  SextInst(Operand ptr) : Instruction(Int64Type::NewInt64TypeGet(), Op::Sext)
  {
    add_use(ptr);
  }

  SextInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class TruncInst : public Instruction
{
public:
  TruncInst(Type *_tp) : Instruction(_tp, Op::Trunc) {};
  TruncInst(Operand ptr) : Instruction(IntType::NewIntTypeGet(), Op::Trunc)
  {
    add_use(ptr);
  }

  TruncInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class MaxInst : public Instruction
{
public:
  MaxInst(Type *_tp) : Instruction(_tp, Op::Max) {};
  MaxInst(Operand _A, Operand _B)
  {
    add_use(_A);
    add_use(_B);
    id = Op::Max;
  }

  MaxInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class MinInst : public Instruction
{
public:
  MinInst(Type *_tp) : Instruction(_tp, Op::Min) {};
  MinInst(Operand _A, Operand _B)
  {
    add_use(_A);
    add_use(_B);
    id = Op::Min;
  }

  MinInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class SelectInst : public Instruction
{
public:
  SelectInst(Type *_tp) : Instruction{_tp, Op::Select} {}
  SelectInst(Operand _cond, Operand _true, Operand _false)
      : Instruction(_true->GetType(), Op::Select)
  {
    add_use(_cond);
    add_use(_true);
    add_use(_false);
  }

  SelectInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

// 前端给框架？中端写自己要用的？
class PhiInst : public Instruction
{
  int oprandNum;
  PhiInst(Type *_tp) : oprandNum(0), Instruction(_tp, Op::Phi) {}
  PhiInst(Instruction *BeforeInst, Type *_tp) : oprandNum(0), Instruction(_tp, Op::Phi) {}
  PhiInst(Instruction *BeforeInst) : oprandNum(0) { id = Op::Phi; }

  PhiInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class GepInst : public Instruction
{
public:
  GepInst(Type *_tp) : Instruction(_tp, Op::Gep) {}
  GepInst(Operand base)
  {
    add_use(base);
    id = Op::Gep;
  }
  // 有arg
  GepInst(Operand base, std::vector<Operand> &args)
  {
    add_use(base);
    for (auto &&i : args)
      add_use(i);
    id = Op::Gep;
  }
  void AddArg(Value *arg) { add_use(arg); }
  Type *GetType(); // 暂未实现
  std::vector<Operand> GetIndexs()
  {
    std::vector<Operand> indexs;
    for (int i = 1; i < useruselist.size(); i++)
      indexs.push_back(useruselist[i]->GetValue());
    return indexs;
  }

  GepInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};