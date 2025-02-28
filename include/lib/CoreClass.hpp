#pragma once
#include <cassert>
#include <string>
#include <vector>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include "Type.hpp"
#include "Singleton.hpp"
#include "MyList.hpp"

class Use;
class User;
class Value;
class Instruction;
class BasicBlock;
class Function;
class Module;

class ValUseList;
using Operand = Value *;
// ValUseList为Value类所使用的Use管理数据结构:类
// UserUseList为User类所使用的Use管理数据结构:vector

// 所有都设为public:,后期再来改

class Use
{
public:
  // 使用者
  User *user = nullptr;
  // 被使用者
  Value *usee = nullptr;
  // 下一个Use
  Use *next = nullptr;
  // 管理这个Use的指针 or 上一个Use
  Use **prev = nullptr;

  // 禁止无参构造
  Use() = delete;
  // 构造赋值
  Use(User *_user, Value *_usee);
  // 析构删除
  ~Use()
  {
    RemoveFromValUseList(user);
  };

  // 设置&获取user&usee
  // Set返回指针的引用
  User *&SetUser() { return user; }
  Value *&SetValue() { return usee; }
  Value *GetValue() { return usee; }
  User *GetUser() { return user; }

  // 定义具体的删除操作
  void RemoveFromValUseList(User *_user);
};

// 如果仅定义一个Value中的Use* UseList，不方便管理，长度等等
// 包含迭代器类，用于遍历管理的Use
// dh: is UesrList, value finds ----> User 
class ValUseList
{
public:
  int size = 0;
  Use *head = nullptr;

  // 默认构造
  ValUseList() = default;

  // 遍历UseList的Use
  class iterator
  {
    // 用于存储当前元素的指针cur
    Use *cur;

  public:
    explicit iterator(Use *_cur) : cur(_cur) {}

    iterator &operator++()
    {
      cur = cur->next;
      return *this;
    }

    // 操作符重载：*、==、！=
    // 解引用*,直接访问Use而不是Use*
    Use *operator*() { return cur; }
    //==
    bool operator==(const iterator &other) const { return cur == other.cur; }
    // ！=
    bool operator!=(const iterator &other) const { return cur != other.cur; }
  };

  // 给该User添加Use
  void push_front(Use *_use)
  {
    assert(_use != nullptr);
    size++;
    _use->next = head;
    if (head)
      head->prev = &(_use->next);
    _use->prev = &head;
    head = _use;
  }

  // ValUseList常用操作：
  // 判空
  bool is_empty() { return head == nullptr; }
  // 获取头尾
  Use *&front() { return head; }
  Use *back()
  {
    if (is_empty())
      return nullptr;
    Use *current = head;
    while (current->next != nullptr)
      current = current->next;
    return current;
  }
  // 获取长度
  int &GetSize() { return size; }
  // 头尾
  iterator begin() const { return iterator(this->head); }
  iterator end() const { return iterator(nullptr); }
  // eg:for (auto it = list.begin(); it != list.end(); ++it)

  void print() const // 调试
  {
    std::cout << "ValUseList: size = " << size << "\n";
    for (auto it = begin(); it != end(); ++it)
    {
      std::cout << "Use: " << *it << "\n";
    }
  }
  void clear()
  {
    Use *current = head;
    while (current)
    {
      Use *nextUse = current->next;
      delete current;
      current = nextUse;
    }
    head = nullptr;
    size = 0;
  }
};


class Value
{
public:
  // (Value) this is the key to find Users 
  ValUseList valuselist;
  std::string name;
  Type *type;
  int version;
  virtual bool isGlobal() { return false; }
  virtual bool isConst() { return false; }

  // 构造至少需要类型，可以不要value
  Value() = delete;
  Value(Type *_type) : type(_type), version(0)
  {
    name = "t";
    // name += std::to_string(Singleton<Module>().IR_number("."));
    //  结果：t1,t2,t3
    //  单例
  }
  Value(Type *_type, const std::string &_name) : type(_type), name(_name), version(0) {}
  Value::~Value()
  {
    while (!valuselist.is_empty())
      delete valuselist.front()->user;
  }

  // 基本操作：获取&设置各种值
  virtual Type *GetType() const { return type; };
  IR_DataType GetTypeEnum() const { return GetType()->GetTypeEnum(); }
  const std::string &GetName() const { return name; }
  void SetName(const std::string &_name) { this->name = _name; }
  void SetType(Type *_type) { type = _type; }
  ValUseList &GetValUseList() { return valuselist; };
  int GetValUseListSize() { return valuselist.GetSize(); }
  void SetVersion(int new_version) { version = new_version; }
  int GetVersion() const { return version; }

  // 克隆，以Value*形式返回
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping)
  {
    if (isGlobal())
      return this;
    if (mapping.find(this) != mapping.end())
      return mapping[this];
    Value *newval = new Value(this->GetType());
    mapping[this] = newval;
    return newval;
  }

  void print()
  {
    if (isConst())
      std::cout << GetName();
    else if (isGlobal())
      std::cout << "@" << GetName();
    else if (auto temp = dynamic_cast<Function *>(this))
      std::cout << "@" << temp->GetName();
    // else if (auto temp = dynamic_cast<BuildInFunction *>(this))
    //   std::cout << "@" << temp->GetName();
    else if (GetName() == "undef")
      std::cout << GetName();
    else
      std::cout << "%" << GetName();
  }

  void add_use(Use *_use) { valuselist.push_front(_use); }
};


class User : public Value
{
public:
  using UsePtr = std::unique_ptr<Use>;
  using UseList = std::vector<UsePtr>;
  UseList useruselist;

  User() = default;
  explicit User(Type *_type) : Value(_type) {};
  virtual ~User() = default;

  // 将User的指针转换为 Value *
  virtual Value *GetDef() { return dynamic_cast<Value *>(this); }
  // 获取Use的序号
  int GetUseIndex(Use *_use)
  {
    assert(_use != nullptr && "Use pointer cannot be null");
    for (int i = 0; i < useruselist.size(); i++)
    {
      if (useruselist[i].get() == _use)
        return i;
    }
    assert(0);
  }
  UseList &GetUserUseList() { return this->useruselist; } // useruselist会设为private

  virtual void add_use(Value *_value)
  {
    assert(_value != nullptr && "Value pointer cannot be null");
    for (const auto &use : useruselist)
    {
      if (use->GetValue() == _value)
      {
        return; // if存在，直接返回
      }
    }
    useruselist.push_back(std::make_unique<Use>(this, _value));
  }
  bool remove_use(Use *_use)
  {
    assert(_use != nullptr && "Use pointer cannot be null");
    for (auto it = useruselist.begin(); it != useruselist.end(); ++it)
    {
      if (it->get() == _use)
      {
        useruselist.erase(it);
        return true; // 是否成功移除
      }
    }
    return false;
  }
  void clear_use() { useruselist.clear(); }

  // 默认调用Value的print
  virtual void print() = 0;
  // clone
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping) override
  {
    auto it = mapping.find(this);
    assert(it != mapping.end() && "User not copied!");
    // 获取映射的目标对象并验证其类型
    auto to = dynamic_cast<User *>(it->second);
    if (!to)
    {
      throw std::runtime_error("Mapping points to a non-User object!");
    }
    to->useruselist.reserve(useruselist.size());
    for (auto &use : useruselist)
    {
      to->add_use(use->GetValue()->clone(mapping));
    }

    return to;
  }
  bool is_empty() const { return useruselist.empty(); }
  size_t GetUserUseListSize() const { return useruselist.size(); }

  // 是否需要遍历所有的 Use的成员函数，用到再写
};

class Instruction : public User
{
public:
  enum Op
  {
    None,
    // Terminators
    UnCond,
    Cond,
    Ret,
    // Memory
    Alloca,
    Load,
    Store,
    Memcpy,
    // Binary
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Eq,
    Ne,
    Ge,
    L,
    Le,
    G,
    // Other
    Gep,
    Phi,
    Call,
    Zext,
    Sext,
    Trunc,
    FP2SI,
    SI2FP,
    BinaryUnknown,
    Max,
    Min,
    Select
  };
  Op id; // 指令类型

  Instruction();
  Instruction(Type *_type, Op _Op) : User(_type), id(_Op) {}
  // Instruction(Type *_type) : User(_type) {}

  Op GetInstId() const { return id; }

  // 判断是否为结束指令
  bool IsTerminateInst() const
  {
    return id == Op::UnCond || id == Op::Cond || id == Op::Ret;
  }
  // 判断是否为二元操作符
  bool IsBinary() const
  {
    return id >= Op::Add && id <= Op::G;
  }
  // 是否为内存相关指令
  bool IsMemoryInst() const
  {
    return id >= Op::Alloca && id <= Op::Memcpy;
  }
  // 是否为类型转换指令
  bool IsCastInst() const
  {
    return id >= Op::Zext && id <= Op::SI2FP;
  }

  void add_use(Value *_value) override
  {
    assert(_value && "Cannot add a null operand!");
    User::add_use(_value);
  }
  virtual void print() override
  {
    std::cout << "Instruction: " << OpToString(id) << std::endl;
  }
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping) override
  {
    auto it = mapping.find(this);
    assert(it != mapping.end() && "Instruction not copied!");
    auto to = dynamic_cast<Instruction *>(it->second);
    if (!to)
    {
      throw std::runtime_error("Mapping points to a non-Instruction object!");
    }
    for (auto &use : GetUserUseList())
    {
      to->add_use(use->GetValue()->clone(mapping));
    }
    return to;
  }

  // 判断是否等于另一条指令
  bool operator==(Instruction &other)
  {
    return id == other.id && GetUserUseList().size() == other.GetUserUseList().size();
  }
  // 获取指定索引的操作数
  Value *GetOperand(size_t idx)
  {
    assert(idx < GetUserUseList().size() && "Operand index out of range!");
    return GetUserUseList()[idx]->GetValue();
  }
  // 将指令类型转换为字符串,便于调试，如果写子类里，此函数可删
  static const char *OpToString(Op op)
  {
    switch (op)
    {
    case Op::None:
      return "None";
    case Op::UnCond:
      return "UnCond";
    case Op::Cond:
      return "Cond";
    case Op::Ret:
      return "Ret";
    case Op::Alloca:
      return "Alloca";
    case Op::Load:
      return "Load";
    case Op::Store:
      return "Store";
    case Op::Memcpy:
      return "Memcpy";
    case Op::Add:
      return "Add";
    case Op::Sub:
      return "Sub";
    case Op::Mul:
      return "Mul";
    case Op::Div:
      return "Div";
    case Op::Mod:
      return "Mod";
    case Op::And:
      return "And";
    case Op::Or:
      return "Or";
    case Op::Xor:
      return "Xor";
    case Op::Eq:
      return "Eq";
    case Op::Ne:
      return "Ne";
    case Op::Ge:
      return "Ge";
    case Op::L:
      return "L";
    case Op::Le:
      return "Le";
    case Op::G:
      return "G";
    case Op::Gep:
      return "Gep";
    case Op::Phi:
      return "Phi";
    case Op::Call:
      return "Call";
    case Op::Zext:
      return "Zext";
    case Op::Sext:
      return "Sext";
    case Op::Trunc:
      return "Trunc";
    case Op::FP2SI:
      return "FP2SI";
    case Op::SI2FP:
      return "SI2FP";
    case Op::BinaryUnknown:
      return "BinaryUnknown";
    case Op::Max:
      return "Max";
    case Op::Min:
      return "Min";
    case Op::Select:
      return "Select";
    default:
      return "Unknown";
    }
  }
};

// 示例子类指令，继承自Instruction,放到CFG实现
// class AllocaInst : public Instruction
// {
// }; //...

// BasicBlock管理Instruction和Function管理BasicBlock都提供了两种数据结构
// 块内是是实现给vector的，双向链表直接继承mylist的，有操作在MyList文件里写
// 使用的时候根据自己要实现的功能选择合适的数据结构
class BasicBlock : public Value, public List<BasicBlock, Instruction>, public Node<Function, BasicBlock>
{
public:
  int LoopDepth;  // 嵌套深度
  bool visited;   // 是否被访问过
  int index;      // 基本块序号
  bool reachable; // 是否可达
  int size_Inst = 0;
  // BasicBlock包含Instruction
  using InstPtr = std::unique_ptr<Instruction>;
  // 当前基本块的指令
  std::vector<InstPtr> instructions;
  // 前驱&后续基本块列表
  std::vector<BasicBlock *> PredBlocks = {};
  std::vector<BasicBlock *> NextBlocks = {};

  std::vector<InstPtr> &GetInsts() { return instructions; }

  BasicBlock() : Value(VoidType::NewVoidTypeGet()), LoopDepth(0), visited(false), index(0), reachable(false) {}

  virtual ~BasicBlock() = default;

  virtual void init_Insts()
  {
    instructions.clear();
    // LoopDepth = 0;
    // visited = false;
    // index = 0;
    // reachable = false;
    NextBlocks.clear();
    PredBlocks.clear();
  }
  // 复制mylist
  BasicBlock *clone(std::unordered_map<Operand, Operand> &mapping) override
  {
    if (mapping.find(this) != mapping.end())
      return dynamic_cast<BasicBlock *>(mapping[this]);
    auto temp = new BasicBlock();
    mapping[this] = temp;
    for (auto i : (*this))
      temp->push_back(dynamic_cast<Instruction *>(i->clone(mapping)));
    return temp;
  }
  // 同时复制vector和mylist
  //  BasicBlock *BasicBlock::clone(std::unordered_map<Operand, Operand> &) override { return this; };

  virtual void print()
  {
    // vector
    std::cout << "BasicBlock " << GetName() << " (Index: " << index << ", LoopDepth: " << LoopDepth << "):\n";
    for (const auto &inst : instructions)
    {
      std::cout << "  ";
      inst->print();
    }

    // List
    //  std::cout << GetName() << ":\n";
    //  for (auto i : (*this))
    //  {
    //    std::cout << "  ";
    //    i->print();
    //  }
  }
  // 获取后继基本块列表
  std::vector<BasicBlock *> GetNextBlocks() const { return NextBlocks; }
  // 获取前驱基本块
  const std::vector<BasicBlock *> &GetPredBlocks() const { return PredBlocks; }
  // 添加后继基本块
  void AddNextBlock(BasicBlock *block) { NextBlocks.push_back(block); }
  // 添加前驱基本块
  void AddPredBlock(BasicBlock *pre) { PredBlocks.push_back(pre); }
  // 移除前驱基本块
  void RemovePredBlock(BasicBlock *pre)
  {
    // 首先从PredBlocks中删除，if最后没用到该功能，可删
    PredBlocks.erase(
        std::remove(PredBlocks.begin(), PredBlocks.end(), pre),
        PredBlocks.end());
    // to be...从链表中删除：管理的instruction和被Function管理
  }

  bool is_empty_Insts() const { return instructions.empty(); }
  // 获取基本块的最后一条指令
  Instruction *GetLastInsts() const
  {
    return is_empty_Insts() ? nullptr : instructions.back().get();
  }
  // 替换后继块中的某个基本块
  void ReplaceNextBlock(BasicBlock *oldBlock, BasicBlock *newBlock)
  {
    for (auto &block : NextBlocks)
    {
      if (block == oldBlock)
      {
        block = newBlock;
        return;
      }
    }
  }
  // 替换前驱块中的某个基本块
  void ReplacePreBlock(BasicBlock *oldBlock, BasicBlock *newBlock)
  {
    for (auto &block : PredBlocks)
    {
      if (block == oldBlock)
      {
        block = newBlock;
        return;
      }
    }
  }

  // 暂未实现，只有声明
  Operand GenerateBinaryInst(BasicBlock *_BB, Operand _A,
                             BinaryInst::Operation _op, Operand _B);
};

// 既提供了vector线性管理BasicBlock，又实现了双向链表
class Function : public Value, public List<Function, BasicBlock>
{
public:
  // 参数和基本块//Function包含BasicBlock
  using ParamPtr = std::unique_ptr<Value>;
  using BBPtr = std::unique_ptr<BasicBlock>;
  std::vector<ParamPtr> params;
  std::vector<BBPtr> BBs;
  std::string id;
  int size_BB = 0;

  Function(IR_DataType _type, const std::string &_id)
      : Value(GetTypeFromIRDataType(_type)), id(_id)
  {
    // 构造默认vector和mylist双开
    push_back(new BasicBlock());
  }
  ~Function() = default;

  std::vector<ParamPtr> &GetParams() { return params; }
  std::vector<BBPtr> &GetBBs() { return BBs; }
  auto begin() { return BBs.begin(); }
  auto end() { return BBs.end(); }
  void Function::print()
  {
    std::cout << "define ";
    type->print();
    std::cout << " @" << name << "(";
    for (auto &i : params)
    {
      i->GetType()->print();
      std::cout << " %" << i->GetName();
      if (i.get() != params.back().get())
        std::cout << ", ";
    }
    std::cout << "){\n";
    for (auto &BB : (*this)) // 链表打印
      BB->print();
    std::cout << "}\n";
  }
  virtual Function *clone(std::unordered_map<Value *, Value *> &) override { return this; };

  // 单指vector加入
  void add_BBs(BasicBlock *BB)
  {
    BBs.push_back(std::unique_ptr<BasicBlock>(BB));
    size_BB++;
  }
  // 同时操作mylist和vector
  void push_both_BB(BasicBlock *BB)
  {
    add_BBs(BB);
    push_back(BB);
  }
  // 中插
  void Insert_BBs(BasicBlock *BB, size_t pos)
  {
    if (pos > BBs.size())
    {
      pos = BBs.size(); // 防越界
    }
    BBs.insert(BBs.begin() + pos, std::unique_ptr<BasicBlock>(BB));
    size_BB++;
  }
  void Insert_BB(BasicBlock *pred, BasicBlock *succ, BasicBlock *insert);
  void Insert_BB(BasicBlock *curr, BasicBlock *insert);

  void Remove_BBs(BasicBlock *BB)
  {
    auto it = std::find_if(BBs.begin(), BBs.end(),
                           [BB](const BBPtr &ptr)
                           { return ptr.get() == BB; });
    if (it != BBs.end())
    {
      BBs.erase(it);
      size_BB--;
    }
  }

  void init_BBs()
  {
    BBs.clear();
    size_BB = 0;
  }
  // 查找基本块
  BasicBlock *Find_BBs(const std::string &name)
  {
    for (const auto &BB : BBs)
    {
      if (BB->name == name)
      {
        return BB.get();
      }
    }
    return nullptr;
  }
  void push_param();
  std::vector<BasicBlock *> GetRetBlock();
};

class Module
{
public:
  // Module包含Function
  using FunctionPtr = std::unique_ptr<Function>;
  std::vector<FunctionPtr> functions;
  std::vector<FunctionPtr> &GetFuncTion() { return functions; }

  Module() = default;
  ~Module() = default;

  // 添加函数到模块
  void push_func(FunctionPtr func)
  {
    if (func)
      functions.push_back(std::move(func));
  }
  Function *GetMain()
  {
    for (auto &func : functions)
    {
      if (func && func->GetName() == "main")
      {
        return func.get();
      }
    }
    return nullptr;
  }
};