#pragma once
#include "SymbolTable.hpp"
#include "Singleton.hpp"
#include "CoreClass.hpp"
#include <set>
#include <unordered_set>
#include <algorithm>
#include <vector>
class BasicBlock;
class Function;

class InitVal;
class InitVals;

class Initializer : public Value, public std::vector<Operand>
{
public:
  Initializer(Type *_tp);
  void Var2Store(BasicBlock *block, const std::string &name, std::vector<int> &gep_data);
  Operand GetInitVal(std::vector<int> &idx, int dep = 0);
  void print()
  {
    if (size() == 0)
    {
      std::cout << "zeroinitializer";
      return;
    }
    std::cout << " [";
    int limi = dynamic_cast<ArrayType *>(type)->GetNum();
    for (int i = 0; i < limi; i++)
    {
      dynamic_cast<ArrayType *>(type)->GetSubType()->print();
      if (i < this->size())
      {
        std::cout << " ";
        if (auto inits = dynamic_cast<Initializer *>((*this)[i]))
          inits->print();
        else
          (*this)[i]->print();
      }
      else
        std::cout << " zeroinitializer";
      if (i != limi - 1)
        std::cout << ", ";
    }
    std::cout << "]";
  }
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
  void print()
  {
    Value::print();
    if (usage == GlobalVar)
      std::cout << " = global ";
    else if (usage == Constant)
      std::cout << " = constant ";
    else
      return;
    auto tp = dynamic_cast<PointerType *>(GetType());
    assert(tp != nullptr && "Variable Must Be a Pointer Type to Inner Content!");
    tp->GetSubType()->print();
    std::cout << " ";
    if (useruselist.size() != 0)
    {
      auto init = GetOperand(0);
      if (auto array_init = dynamic_cast<Initializer *>(init))
        array_init->print();
      else
        init->print();
    }
    else
      std::cout << "zeroinitializer";
    std::cout << '\n';
  }
};

class UndefValue : public ConstantData
{
  UndefValue(Type *Ty) : ConstantData(Ty) { name = "undef"; }

public:
  static UndefValue *Get(Type *Ty);

  virtual UndefValue *clone(std::unordered_map<Operand, Operand> &) override;
  void print()
  {
    dynamic_cast<Value *>(this)->print();
    return;
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
  void print() final
  {
    Value::print();
    std::cout << " = load ";
    type->print();
    std::cout << ", ";
    for (auto &i : useruselist)
    {
      i->GetValue()->GetType()->print();
      std::cout << " ";
      i->GetValue()->print();
      if (i.get() != useruselist.back().get())
        std::cout << ", ";
    }
    std::cout << '\n';
  }
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
  void print() final
  {
    std::cout << "store ";
    for (auto &i : useruselist)
    {
      i->GetValue()->GetType()->print();
      std::cout << " ";
      i->GetValue()->print();
      if (i.get() != useruselist.back().get())
        std::cout << ", ";
    }
    std::cout << '\n';
  }
};

class AllocaInst : public Instruction
{
public:
  bool AllZero = false;
  bool HasStored = false;

  AllocaInst(Type *_tp);

  bool isUsed();
  AllocaInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final
  {
    Value::print();
    std::cout << " = alloca ";
    dynamic_cast<PointerType *>(type)->GetSubType()->print();
    std::cout << "\n";
  }
};

class CallInst : public Instruction
{
public:
  CallInst(Type *_tp);
  CallInst(Value *_func, std::vector<Operand> &_args, std::string label = "");
  Value* GetCalledFunction() const { return CalledFunction; }
  CallInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final
  {
    if (type != VoidType::NewVoidTypeGet())
    {
      Value::print();
      std::cout << " = ";
    }
    std::cout << "call ";
    for (auto &i : useruselist)
    {
      i->GetValue()->GetType()->print();
      std::cout << " ";
      i->GetValue()->print();
      if (i.get() == useruselist.front().get())
        std::cout << "(";
      else if (i.get() != useruselist.back().get())
        std::cout << ", ";
    }
    std::cout << ")\n";
  }
private:
    Value* CalledFunction;
};

class RetInst : public Instruction
{
public:
  RetInst();
  RetInst(Type *_tp);
  RetInst(Operand op);

  Operand GetDef() final;

  RetInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    std::cout << "ret ";
    for (auto &i : useruselist)
    {
      i->GetValue()->GetType()->print();
      std::cout << " ";
      i->GetValue()->print();
      std::cout << " ";
    }
    if (useruselist.empty())
      std::cout << "void";
    std::cout << '\n';
  }
};

class CondInst : public Instruction
{
public:
  CondInst(Type *_tp);
  CondInst(Operand _cond, BasicBlock *_true, BasicBlock *_false);

  Operand GetDef() final;

  CondInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    std::cout << "br ";
    bool flag = 0;
    for (auto &i : useruselist)
    {
      if (flag == 0)
      {
        i->GetValue()->GetType()->print();
        std::cout << " ";
        flag = 1;
      }
      else
        std::cout << "label ";
      i->GetValue()->print();
      if (i.get() != useruselist.back().get())
        std::cout << ", ";
    }
    std::cout << '\n';
  }
};

class UnCondInst : public Instruction
{
public:
  UnCondInst(Type *_tp);
  UnCondInst(BasicBlock *_BB);

  Operand GetDef() final;

  UnCondInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    std::cout << "br ";
    for (auto &i : useruselist)
    {
      std::cout << "label ";
      i->GetValue()->print();
      std::cout << " ";
    }
    std::cout << '\n';
  }
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
  BinaryInst(Operand _A, Operation __op, Operand _B, bool Atom = false);
  BinaryInst *clone(std::unordered_map<Operand, Operand> &) override;

  bool IsAtomic() const {return Atomic;}//hu1 add it
  
  const Operation &GetOp() { return op; }
  void print() final
  {
    Value::print();
    IR_DataType tp = useruselist[0]->GetValue()->GetTypeEnum();
    std::cout << " = ";
    switch (op)
    {
    case BinaryInst::Op_Add:
      if (!this->Atomic)
      {
        if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_INT)
          std::cout << "add ";
        else
          std::cout << "fadd ";
      }
      else
      {
        std::cout << "atomicadd ";
      }
      break;
    case BinaryInst::Op_Sub:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "sub ";
      else
        std::cout << "fsub ";
      break;
    case BinaryInst::Op_Mul:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "mul ";
      else
        std::cout << "fmul ";
      break;
    case BinaryInst::Op_Div:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "sdiv ";
      else
        std::cout << "fdiv ";
      break;
    case BinaryInst::Op_Mod:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "srem ";
      else
        std::cout << "frem "; /// 应该不存在
      break;
    case BinaryInst::Op_And:
      std::cout << "and ";
      break;
    case BinaryInst::Op_Or:
      std::cout << "or ";
      break;
    case BinaryInst::Op_Xor:
      std::cout << "xor ";
      break;
    case BinaryInst::Op_E:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "i";
      else
        std::cout << "f";
      std::cout << "cmp ";
      if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_Float)
        std::cout << "u";
      std::cout << "eq ";
      break;
    case BinaryInst::Op_NE:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "i";
      else
        std::cout << "f";
      std::cout << "cmp ";
      if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_Float)
        std::cout << "u";
      std::cout << "ne ";
      break;
    case BinaryInst::Op_G:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "i";
      else
        std::cout << "f";
      std::cout << "cmp ";
      if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_Float)
        std::cout << "ugt ";
      else
        std::cout << "sgt ";
      break;
    case BinaryInst::Op_GE:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "i";
      else
        std::cout << "f";
      std::cout << "cmp ";
      if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_Float)
        std::cout << "uge ";
      else
        std::cout << "sge ";
      break;
    case BinaryInst::Op_L:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "i";
      else
        std::cout << "f";
      std::cout << "cmp ";
      if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_Float)
        std::cout << "ult ";
      else
        std::cout << "slt ";
      break;
    case BinaryInst::Op_LE:
      if (tp == IR_Value_INT || tp == IR_INT_64)
        std::cout << "i";
      else
        std::cout << "f";
      std::cout << "cmp ";
      if (useruselist[0]->GetValue()->GetTypeEnum() == IR_Value_Float)
        std::cout << "ule ";
      else
        std::cout << "sle ";
      break;
    default:
      break;
    }
    if (this->Atomic)
    {
      useruselist[0]->GetValue()->GetType()->print();
      std::cout << " ";
      useruselist[0]->GetValue()->print();
      std::cout << ", ";
      useruselist[1]->GetValue()->GetType()->print();
      std::cout << " ";
      useruselist[1]->GetValue()->print();
      std::cout << '\n';
    }
    else
    {
      useruselist[0]->GetValue()->GetType()->print();
      std::cout << " ";
      useruselist[0]->GetValue()->print();
      std::cout << ", ";
      useruselist[1]->GetValue()->print();
      std::cout << '\n';
    }
  }
};

class ZextInst : public Instruction
{
public:
  ZextInst(Type *_tp);
  ZextInst(Operand ptr);

  ZextInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = zext i1 ";
    useruselist[0]->GetValue()->print();
    std::cout << " to i32\n";
  }
};

class SextInst : public Instruction
{
public:
public:
  SextInst(Type *_tp);
  SextInst(Operand ptr);

  SextInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = sext i32 ";
    useruselist[0]->GetValue()->print();
    std::cout << " to i64\n";
  }
};

class TruncInst : public Instruction
{
public:
  TruncInst(Type *_tp);
  TruncInst(Operand ptr);

  TruncInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = trunc i64 ";
    useruselist[0]->GetValue()->print();
    std::cout << " to i32\n";
  }
};

class MaxInst : public Instruction
{
public:
  MaxInst(Type *_tp);
  MaxInst(Operand _A, Operand _B);

  MaxInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = call ";
    this->type->print();
    if (this->type->GetTypeEnum() == IR_Value_INT)
      std::cout << " @max(";
    else if (this->type->GetTypeEnum() == IR_Value_Float)
      std::cout << " @fmax(";
    useruselist[0]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[0]->GetValue()->print();
    std::cout << ", ";
    useruselist[1]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[1]->GetValue()->print();
    std::cout << ")\n";
  }
};

class MinInst : public Instruction
{
public:
  MinInst(Type *_tp);
  MinInst(Operand _A, Operand _B);

  MinInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = call ";
    this->type->print();
    if (this->type->GetTypeEnum() == IR_Value_INT)
      std::cout << " @min(";
    else if (this->type->GetTypeEnum() == IR_Value_Float)
      std::cout << " @fmin(";
    useruselist[0]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[0]->GetValue()->print();
    std::cout << ", ";
    useruselist[1]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[1]->GetValue()->print();
    std::cout << ")\n";
  }
};

class SelectInst : public Instruction
{
public:
  SelectInst(Type *_tp);
  SelectInst(Operand _cond, Operand _true, Operand _false);

  SelectInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = select ";
    useruselist[0]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[0]->GetValue()->print();
    std::cout << ", ";
    useruselist[1]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[1]->GetValue()->print();
    std::cout << ", ";
    useruselist[2]->GetValue()->GetType()->print();
    std::cout << " ";
    useruselist[2]->GetValue()->print();
    std::cout << '\n';
  }
};

class GepInst : public Instruction
{
public:
  GepInst(Type *_tp);
  GepInst(Operand base);
  GepInst(Operand base, std::vector<Operand> &args);

  void AddArg(Value *arg);
  Type *GetType();
  std::vector<Operand> GetIndexs();

  GepInst *clone(std::unordered_map<Operand, Operand> &) override;
  void print() final
  {
    Value::print();
    std::cout << " = getelementptr inbounds ";
    dynamic_cast<HasSubType *>(useruselist[0]->GetValue()->GetType())
        ->GetSubType()
        ->print();
    for (int i = 0; i < useruselist.size(); i++)
    {
      std::cout << ", ";
      useruselist[i]->GetValue()->GetType()->print();
      std::cout << " ";
      useruselist[i]->GetValue()->print();
    }
    std::cout << '\n';
  }
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
  void print() final
  {
    Value::print();
    std::cout << " = fptosi ";
    for (auto &i : useruselist)
    {
      i->GetValue()->GetType()->print();
      std::cout << " ";
      i->GetValue()->print();
      std::cout << " ";
    }
    std::cout << "to ";
    type->print();
    std::cout << "\n";
  }
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
  void print() final
  {
    Value::print();
    std::cout << " = sitofp ";
    for (auto &i : useruselist)
    {
      i->GetValue()->GetType()->print();
      std::cout << " ";
      i->GetValue()->print();
      std::cout << " ";
    }
    std::cout << "to ";
    type->print();
    std::cout << '\n';
  }
};

Operand ToFloat(Operand op, BasicBlock *block);
Operand ToInt(Operand op, BasicBlock *block);

/// dh 来实现这个
class PhiInst : public Instruction
{
  using UsePtr = std::unique_ptr<Use>;

protected:
  std::vector<UsePtr> uselist;

private:
  int oprandNum; // 记录操作数的数目
  std::vector<Value *> Incomings;
  // std::map<int, std::pair<Value *, BasicBlock *>> PhiRecord; // phi [val1,BB1] [val2,BB2]; phi语句类似如此 我要记录这个
  std::vector<BasicBlock *> IncomingBlocks;
  std::map<Use *, int> UseRecord;

public:
  PhiInst(Type *_tp);
  PhiInst(Instruction *BeforeInst, Type *_tp);
  PhiInst(Instruction *BeforeInst);
  std::map<int, std::pair<Value *, BasicBlock *>> PhiRecord;
  // 暂时使不报错//添完整了ww
  PhiInst *clone(std::unordered_map<Operand, Operand> &mapping) override;
  void print() final;

  static PhiInst *Create(Instruction *BeforeInst, BasicBlock *currentBB, std::string Name = "");
  static PhiInst *Create(Instruction *BeforeInst, BasicBlock *currentBB, Type *type, std::string Name = "");
  static PhiInst *Create(Type *type);

  // 增加前驱 to phiInst 的值
  void addIncoming(Value *Incoming, BasicBlock *PreBB);
  int getNumIncomingValues() { return oprandNum; } //
  BasicBlock *getIncomingBlock(int num);
  Value *getIncomingValue(int num);
  std::vector<Value *> &RecordIncomingValsA_Blocks();
  bool IsReplaced();

  // unrolling
  Value *ReturnValIn(BasicBlock *bb);
  void Del_Incomes(int CurrentNum);
  void FormatPhi();
  void ModifyBlock(BasicBlock *Old, BasicBlock *New);
  void ReplaceVal(Use *use, Value *new_val);
  // void RSUW(Use *u, Operand val)//移到基类User了，更通用
  // {
  //   u->RemoveFromValUseList(this);
  //   u->usee = val;
  //   val->add_use(u);
  // }
  // hu1 do it
  // 删去phiinst的一个引用块实现
  //  %r = phi i32 [ %a, %bb ], [ %b, %bb2 ]
  //  ↓
  //  %r = phi i32 [ %b, %bb2 ]
  // 但需要注意,如果删完了之后是空phi了,需要自己删除掉这条指令
  void removeIncomingFrom(BasicBlock *fromBB);

  // 常量传播处理phi函数的,在RAUW里面做的处理
  void PhiProp(Value *old, Value *val);
  void SetIncomingBlock(int index, BasicBlock *bb) {
   IncomingBlocks[index] = bb;
 }
};

// BasicBlock管理Instruction和Function管理BasicBlock都提供了两种数据结构
// 块内是是实现给vector的，双向链表直接继承mylist的，有操作在MyList文件里写
// 使用的时候根据自己要实现的功能选择合适的数据结构
class BasicBlock : public Value, public List<BasicBlock, Instruction>, public Node<Function, BasicBlock>
{
public:
  int index;         // 基本块序号
  int LoopDepth = 0; // 嵌套深度
  bool visited;      // 是否被访问过
  // int index;      // 基本块序号
  bool reachable; // 是否可达
  // BasicBlock包含Instruction
  using InstPtr = std::shared_ptr<Instruction>;
  // 当前基本块的指令
  // std::vector<InstPtr> instructions = {};
  // 前驱&后续基本块列表
  std::vector<BasicBlock *> PredBlocks = {};
  std::vector<BasicBlock *> NextBlocks = {};
  int num = 0;

public:
  // 获取当前基本块的指令
  // 这个东西没有实现，，，是之前的结构，获取Insts要走链表的迭代器迭代
  // std::vector<InstPtr> &GetInsts();
  int &GetIndex() { return index; }
  BasicBlock();          // 构造函数
  virtual ~BasicBlock(); // 析构函数

  // virtual void init_Insts(); // 初始化指令

  // 复制mylist
  BasicBlock *clone(std::unordered_map<Operand, Operand> &mapping) override;

  virtual void print();

  // 操作前驱/后继数组
  std::vector<BasicBlock *> GetNextBlocks() const;
  const std::vector<BasicBlock *> &GetPredBlocks() const;
  void AddNextBlock(BasicBlock *block);
  void AddPredBlock(BasicBlock *pre);
  void RemovePredBlock(BasicBlock *pre);
  void RemoveNextBlock(BasicBlock *pre);

  // 操作链表，待完善 / 或者mylist中去找erase方法实现逻辑
  void RemovePredBB(BasicBlock *pre);

  // 注释了
  bool is_empty_Insts() const; // 判断指令是否为空

  // 获取基本块的最后一条指令
  // 链表最后
  Instruction *GetLastInsts() const;
  Instruction* GetFirstInsts() const;

  // 替换后继块中的某个基本块
  void ReplaceNextBlock(BasicBlock *oldBlock, BasicBlock *newBlock);

  // 替换前驱块中的某个基本块
  void ReplacePreBlock(BasicBlock *oldBlock, BasicBlock *newBlock);

  Operand GenerateBinaryInst(Operand _A, BinaryInst::Operation op, Operand _B);
  static Operand GenerateBinaryInst(BasicBlock *, Operand, BinaryInst::Operation, Operand);
  Operand GenerateSI2FPInst(Operand _A);
  Operand GenerateFP2SIInst(Operand _A);
  Operand GenerateLoadInst(Operand);
  void GenerateStoreInst(Operand, Operand);
  AllocaInst *GenerateAlloca(Type *_tp, std::string name);
  void GenerateCondInst(Operand, BasicBlock *, BasicBlock *);
  void GenerateUnCondInst(BasicBlock *);
  Operand GenerateCallInst(std::string, std::vector<Operand>, int);
  void GenerateRetInst(Operand);
  void GenerateRetInst();
  Operand GenerateGepInst(Operand);
  Operand GenerateZextInst(Operand);
  BasicBlock *GenerateNewBlock();
  BasicBlock *GenerateNewBlock(std::string);
  bool IsEnd(); // 是否划分
  BasicBlock *SplitAt(User *inst);
  //遍历前驱/后继块指令
  void ForEachInstrInPredBlocks(std::function<void(Instruction *)> visitor);
  void ForEachInstrInNextBlocks(std::function<void(Instruction *)> visitor);
  int GetSuccessorCount();
  int GetPredecessorCount();
  Instruction *GetTerminator() const;
  BasicBlock* GetSuccessor(int i) const {
    if (i < 0 || i >= static_cast<int>(NextBlocks.size()))
        return nullptr;
    return NextBlocks[i];
}
};

class BuiltinFunc : public Value
{
  BuiltinFunc(Type *, std::string);

public:
  static bool CheckBuiltin(std::string);
  static BuiltinFunc *GetBuiltinFunc(std::string);
  // static CallInst *BuiltinTransform(CallInst *);
  static Instruction *GenerateCallInst(std::string, std::vector<Operand> args);
  virtual BuiltinFunc *clone(std::unordered_map<Operand, Operand> &) override { return this; }
};

// 既提供了vector线性管理BasicBlock，又实现了双向链表
class Function : public Value, public List<Function, BasicBlock>
{
public:
  using ParamPtr = std::unique_ptr<Value>;
  using BBPtr = std::shared_ptr<BasicBlock>; // 改变了指针类型
private:
  std::vector<ParamPtr> params;
  std::vector<BBPtr> BBs;
  std::pair<size_t,size_t> inlineinfo;
  // std::string id;

public:
  Function(IR_DataType _type, const std::string &_id);
  ~Function() = default;
  enum Tag
  {
    Normal,
    UnrollBody,
    LoopBody,
    ParallelBody,
    BuildIn,
  };
  Tag tag = Normal;
  int num=0;
  bool CmpEqual = false;

  //hu1 add it for SideEffect
  std::set<Value*> Change_Val;//func->C 表示该函数会修改的值(某个参数,全局变量等),用于DSE.函数内联等优化
  bool HasSideEffect = false;//func->H 调用可知是否有副作用

  std::vector<ParamPtr> &GetParams();
  std::vector<BBPtr> &GetBBs();
  // std::vector<BasicBlock*> &GetBasicBlock();
  inline Tag &GetTag() { return tag; }
  std::vector<BasicBlock *> GetRetBlock();
  void print();
  virtual Function *clone(std::unordered_map<Value *, Value *> &) override { return this; }

  // 带BB的都是操作vector，List的相关函数在MyLsit
  void AddBBs(BasicBlock *BB);
  void PushBothBB(BasicBlock *BB);
  void InsertBBs(BasicBlock *BB, size_t pos);
  // 以下两个暂未实现//实现了ww
  // dh: 这两个我需要你帮助维护 bbs:index这个属性
  //vector&mylist都操作
  void InsertBlock(BasicBlock *pred, BasicBlock *succ, BasicBlock *insert);
  void InsertBlock(BasicBlock *curr, BasicBlock *insert);

  void RemoveBBs(BasicBlock *BB);
  void InitBBs();
  void PushParam(std::string, Var *);
  void UpdateParam(Var *var) { params.emplace_back(var); }
  size_t GetSize() { return BBs.size();}
  size_t GetInstructionCount() const;//链表
  std::pair<Value *, BasicBlock *> InlineCall(CallInst *inst, std::unordered_map<Operand, Operand> &OperandMapping);
  std::pair<size_t,size_t>& GetInlineInfo();
  inline void ClearInlineInfo(){inlineinfo.first=inlineinfo.second=0;};

  //封装了一个链表操作ww
  void InsertBlockAfter(BasicBlock* pos, BasicBlock* new_bb); 
  bool isRecursive(bool=true);

  //hu1 add it:对bb的visited进行初始化
  void init_visited_block(){
      for (auto bb : *this) {
        bb->visited = false;
    }
  }

};

class Module : public SymbolTable
{
private:
  using FunctionPtr = std::unique_ptr<Function>;
  using GlobalVariblePtr = std::unique_ptr<Var>;
  std::vector<FunctionPtr> functions;
  std::vector<GlobalVariblePtr> globalvaribleptr;

public:
  Module() = default;
  ~Module() = default;
  // 中端pass
  std::set<Function *> hasInlinedFunc;
  std::set<Function *> inlinedFunc;
  std::set<Function *> Side_Effect_Funcs;

  std::vector<FunctionPtr> &GetFuncTion() { return functions; }
  std::vector<std::unique_ptr<Var>> &GetGlobalVariable() { return globalvaribleptr; }
  void push_func(FunctionPtr func);
  void PushVar(Var *ptr);
  Function *GetMain();
  std::string GetFuncNameEnum(std::string = "");
  Function &GenerateFunction(IR_DataType _tp, std::string _id);

  // 死代码消除，只有声明，中端待定义
  void EraseFunction(Function *func)
  {
  }
  bool EraseDeadFunc();
  void Test()
  {
    for (auto &i : globalvaribleptr)
      i->print();
    for (auto &i : functions)
      i->print();
  }
};