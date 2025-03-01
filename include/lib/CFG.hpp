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


// 前端给框架？中端写自己要用的？
class PhiInst : public Instruction
{
  public:
  int oprandNum;

  PhiInst(Type *_tp);
  PhiInst(Instruction *BeforeInst, Type *_tp);
  PhiInst(Instruction *BeforeInst);

  PhiInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final;
};

