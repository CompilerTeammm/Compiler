#include "CoreClass.hpp"
#include "Type.hpp"
// #include <cassert>
#include <sstream>

// 这边的类都还暂未实现，先放，占位

// 所有常量数据的基类
class ConstantData : public Value
{
public:
  ConstantData() = delete;
  ConstantData(Type *_tp);

  static ConstantData *getNullValue(Type *tp);

  bool isConst() final;
  bool isZero();

  virtual ConstantData *clone(std::unordered_map<Operand, Operand> &mapping) override;
};

class ConstIRBoolean : public ConstantData
{
private:
  bool val;
  bool IsBoolean() const override { return true; }
  ConstIRBoolean(bool _val);

public:
  static ConstIRBoolean *GetNewConstant(bool _val = false);
  bool GetVal();
};

class ConstIRInt : public ConstantData
{
private:
  int val;
  ConstIRInt(int _val);

public:
  static ConstIRInt *GetNewConstant(int _val = 0);
  int GetVal();
};

class ConstIRFloat : public ConstantData
{
private:
  float val;
  ConstIRFloat(float _val);

public:
  static ConstIRFloat *GetNewConstant(float _val = 0);
  float GetVal();
  double GetValAsDouble() const;
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

class UndefValue : public ConstantData
{
  UndefValue(Type *Ty) : ConstantData(Ty) { name = "undef"; }

public:
  static UndefValue *Get(Type *Ty);

  virtual UndefValue *clone(std::unordered_map<Operand, Operand> &) override;
  void print();
};

class Var : public User
{
public:
  enum UsageTag
  {
    GlobalVar,
    Constant,
    Param
  } usage;
  bool ForParallel = false;
  bool isGlobal() final
  {
    if (usage == Param)
      return false;
    else
      return true;
  }
  bool isParam()
  {
    if (usage == Param)
      return true;
    else
      return false;
  }
  Var(UsageTag, Type *, std::string);

  inline Value *GetInitializer()
  {
    if (useruselist.empty())
    {
      return nullptr;
    }
    return useruselist[0]->usee;
  };

  virtual Value *clone(std::unordered_map<Operand, Operand> &) override;
  void print();
};

class BuiltinFunc : public Value
{
  BuiltinFunc(Type *, std::string);

public:
  static bool CheckBuiltin(std::string);
  static BuiltinFunc *GetBuiltinFunc(std::string);
  static CallInst *BuiltinTransform(CallInst *);
  static Instruction *GenerateCallInst(std::string, std::vector<Operand> args);
  virtual BuiltinFunc *clone(std::unordered_map<Operand, Operand> &) override { return this; }
};

// 所有Inst
// 通用的clone,print方法暂时都还没有写
class LoadInst : public Instruction
{
public:
  LoadInst(Type *_tp);
  LoadInst(Operand _src);

  LoadInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;
};

class StoreInst : public Instruction
{
public:
  bool isUsed = false;

  StoreInst(Type *_tp);
  StoreInst(Operand _A, Operand _B);

  virtual void test() {}
  Operand GetDef() final;
  StoreInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;
};

class AllocaInst : public Instruction
{
public:
  bool AllZero = false;
  bool HasStored = false;

  AllocaInst(Type *_tp);

  bool isUsed();
  AllocaInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;
};

class CallInst : public Instruction
{
public:
  CallInst(Type *_tp);
  CallInst(Value *_func, std::vector<Operand> &_args, std::string label = "");

  CallInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;
};

class RetInst : public Instruction
{
public:
  RetInst();
  RetInst(Type *_tp);
  RetInst(Operand op);

  Operand GetDef() final;

  RetInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class CondInst : public Instruction
{
public:
  CondInst(Type *_tp);
  CondInst(Operand _cond, BasicBlock *_true, BasicBlock *_false);

  Operand GetDef() final;

  CondInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class UnCondInst : public Instruction
{
public:
  UnCondInst(Type *_tp);
  UnCondInst(BasicBlock *_BB);

  Operand GetDef() final;

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

class ZextInst : public Instruction
{
public:
  ZextInst(Type *_tp);
  ZextInst(Operand ptr);

  ZextInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class SextInst : public Instruction
{
public:
public:
  SextInst(Type *_tp);
  SextInst(Operand ptr);

  SextInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class TruncInst : public Instruction
{
public:
  TruncInst(Type *_tp);
  TruncInst(Operand ptr);

  TruncInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class MaxInst : public Instruction
{
public:
  MaxInst(Type *_tp);
  MaxInst(Operand _A, Operand _B);

  MaxInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class MinInst : public Instruction
{
public:
  MinInst(Type *_tp);
  MinInst(Operand _A, Operand _B);

  MinInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class SelectInst : public Instruction
{
public:
  SelectInst(Type *_tp);
  SelectInst(Operand _cond, Operand _true, Operand _false);

  SelectInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class GepInst : public Instruction
{
public:
  GepInst(Type *_tp);
  GepInst(Operand base);
  GepInst(Operand base, std::vector<Operand> &args);

  void AddArg(Value *arg);
  Type *GetType(); // 暂未实现
  std::vector<Operand> GetIndexs();

  GepInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

// 类型转换指令
class FP2SIInst : public Instruction
{
public:
  FP2SIInst(Type *_tp) : Instruction(_tp, Op::FP2SI) {};
  FP2SIInst(Operand _A) : Instruction(IntType::NewIntTypeGet(), Op::FP2SI)
  {
    add_use(_A);
  }
  FP2SIInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

class SI2FPInst : public Instruction
{
public:
  SI2FPInst(Type *_tp) : Instruction(_tp, Op::SI2FP) {};
  SI2FPInst(Operand _A) : Instruction(FloatType::NewFloatTypeGet(), Op::SI2FP)
  {
    add_use(_A);
  }
  SI2FPInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

/// dh
// 前端给框架？中端写自己要用的？
class PhiInst : public Instruction
{
public:
  int oprandNum;

  PhiInst(Type *_tp);
  PhiInst(Instruction *BeforeInst, Type *_tp);
  PhiInst(Instruction *BeforeInst);

  static PhiInst *Create(Type *type, int Num, std::string name, BasicBlock *BB);
  PhiInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};