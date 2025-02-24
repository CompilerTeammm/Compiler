#include "CoreClass.hpp"
#include "Type.hpp"

// 这边的类都还暂未实现，先放，占位
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
  BinaryInst(Operand _A, Operation __op, Operand _B, bool Atom = false);
  BinaryInst *clone(std::unordered_map<Operand, Operand> &) override;
  bool IsCmpInst() { return (op - Op_Add) > 7; }
  void print() final;
  void SetOperand(int index, Value *val);
  std::string GetOperation();
  Operation getopration();
  void setoperation(Operation _op) { op = _op; }
  static BinaryInst *CreateInst(Operand _A, Operation __op, Operand _B, User *place = nullptr);
};

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
  ConstIRBoolean(bool);

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
  ConstIRInt(int);

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
  ConstIRFloat(float);

public:
  static ConstIRFloat *GetNewConstant(float = 0);
  float GetVal();
  double GetValAsDouble() const { return static_cast<double>(val); }
};

class ConstPtr : public ConstantData
{
  ConstPtr(Type *);

public:
  static ConstPtr *GetNewConstant(Type *);
};