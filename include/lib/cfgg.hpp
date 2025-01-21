#pragma once
#include <cassert>
#include <string>
#include <vector>
#include <iostream>
#include <memory>

class Use;
class User;
class Value;
class Instruction;
class BasicBlock;
class Function;
class Module;

class UseList;
class Type; // 可以先暂时不管这个类，我还没写
// 所有都设为public:,后期再来改

class Value
{
public:
  UseList uselist;
  std::string name;
  Type *type;
  bool isCloned = false;
  virtual bool isGlobal() { return false; }
  virtual bool isConst() { return false; }

  // 构造至少需要类型，可以不要value
  Value() = delete;
  Value(Type *_type) : type(_type)
  {
    name = "t";
    // name += std::to_string(Singleton<Module>().IR_number("."));
    //  结果：t1,t2,t3
    //  单例
  }
  Value(Type *_type, std::string _name) : type(_type), name(_name) {}
  Value::~Value()
  {
    while (!uselist.is_empty())
      delete uselist.front()->user;
  }

  // 基本操作：获取&设置各种值
  virtual Type *GetType() { return type; };
  std::string Value::GetName() { return name; }
  void Value::SetName(std::string _name) { this->name = _name; }
  void SetType(Type *_type) { type = _type; }
  UseList &GetUselist() { return uselist; };
  int GetUseListSize() { return GetUselist().GetSize(); }

  // 克隆，以Value*形式返回
  virtual Value *clone()
  {
    if (isGlobal())
      return this;
    if (isCloned)
      return this;

    Value *newval = new Value(this->GetType());
    newval->isCloned = true;
    return newval;
  }
  void add_use(Use *_use) { uselist.push_front(_use); }
  void print()
  {
    if (isConst())
      std::cout << GetName();
    else if (isGlobal())
      std::cout << "@" << GetName();
    // else if (auto temp = dynamic_cast<Function *>(this))
    //   std::cout << "@" << temp->GetName();
    // else if (auto temp = dynamic_cast<BuildInFunction *>(this))
    //   std::cout << "@" << temp->GetName();
    else if (GetName() == "undef")
      std::cout << GetName();
    else
      std::cout << "%" << GetName();
  }
};

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
  Use(User *_user, Value *_usee) : user(_user), usee(_usee)
  {
    usee->add_use(this);
  }
  // 析构删除
  ~Use()
  {
    RemoveFromUseList(user);
  };

  // 设置&获取user&usee
  User *&SetUser() { return user; }
  Value *&SetValue() { return usee; }
  Value *GetValue() { return usee; }
  User *GetUser() { return user; }

  // 返回该Use的相关user&usee
  void RemoveFromUseList(User *_user)
  {
    User *&SetUser();
    Value *&SetValue();
    Value *GetValue();
    User *GetUser();
  }
  // 定义具体的删除操作
  void RemoveFromUseList(User *_user)
  {
    assert(_user == user);
    GetValue()->GetUselist().GetSize()--;
    (*prev) = next;
    if (next != nullptr)
      next->prev = prev;
    if (GetValue()->GetUselist().GetSize() == 0 && next != nullptr)
      assert(0);
  }
};

// 如果仅定义一个Value中的Use* UseList，不方便管理，长度等等
// 包含迭代器类，用于遍历管理的Use
class UseList
{
public:
  int size = 0;
  Use *head = nullptr;
  // 遍历UseList的Use
  class iterator
  {
    // 用于存储当前元素的指针cur
    Use *cur;

  public:
    iterator(Use *_cur) : cur(_cur) {}

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

  // 默认构造
  UseList() = default;

  // 给该User添加Use
  void push_front(Use *_use)
  {
    size++;
    _use->next = head;
    if (head)
      head->prev = &(_use->next);
    _use->prev = &head;
    head = _use;
  }

  // UseList常用操作：
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
  // 头遍历
  iterator begin() { return iterator(this->head); }
  // 是否需要尾遍历，需要再加
};

class User : public Value
{
public:
  using UsePtr = std::unique_ptr<Use>;
  std::vector<UsePtr> vuselist;

  User();
  User(Type *_type) : Value(_type) {};
  virtual ~User() = default;

  // 将User的指针）转换为 Value *
  Value *GetDef() { return dynamic_cast<Value *>(this); }
  // 获取Use的序号
  int GetUseIndex(Use *_use)
  {
    for (int i = 0; i < vuselist.size(); i++)
    {
      if (vuselist[i].get() == _use)
        return i;
    }
    assert(0);
  }
  std::vector<UsePtr> &Getvuselist() { return this->vuselist; } // vuselist会设为private
  virtual void add_vuse(Value *_value)
  {
    vuselist.push_back(std::make_unique<Use>(this, _value));
  }
  // 默认调用Value的print
  virtual void print() = 0;
  // clone
  virtual Value *clone() override;
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
  Op id;

  Instruction();
  Instruction(Type *_type, Op _Op);

  bool IsTerminateInst(); //...
  bool IsBinary()
  {
    if (id >= 8 && id <= 21)
      return true;
    return false;
  }

  void add_vuse(Value *_value) override;
  void print() override;
  Op GetInstId() const { return id; }
};

class AllocaInst : public Instruction
{
}; //...

class BasicBlock : public Value
{
public:
  int LoopDepth = 0;      // 嵌套深度
  bool visited = false;   // 是否被访问过
  int index = 0;          // 基本块序号
  bool reachable = false; // 是否可达
  // BasicBlock包含Instruction
  using InstPtr = std::unique_ptr<Instruction>;
  std::vector<InstPtr> &GetInst();

  BasicBlock();
  virtual void clear();
  virtual ~BasicBlock() = default;
  BasicBlock *clone() override;
  void print()
  {
    // 先把迭代器实现了
    //  std::cout << GetName() << ":\n";
    //  for (auto i : (*this))
    //  {
    //    std::cout << "  ";
    //    i->print();
    //  }
  }
  // 后继BB
  std::vector<BasicBlock *> NextBlock;
  std::vector<BasicBlock *> GetNextBlock();
  void AddNextBlock(BasicBlock *block) { this->NextBlock.push_back(block); }
  void RemovePreBlock(BasicBlock *pre);
};

// 管理BasicBlock
class Function : public Value
{
public:
  // 参数和基本块//Function包含BasicBlock
  using ParamPtr = std::unique_ptr<Value>;
  using BBPtr = std::unique_ptr<BasicBlock>;
  std::vector<ParamPtr> params;
  std::vector<BasicBlock *> BBs;
  int size_BB = 0;

  Function(Type _type, std::string _id);

  void print();
  virtual Function *clone() override { return this; };
  std::vector<ParamPtr> &GetParams();
  std::vector<BasicBlock *> &GetBasicBlock() { return BBs; }
  void add_block(BasicBlock *);
  void push_param();
  void init_BB();
  void push_BB(BasicBlock *BB);   // 尾插
  void Insert_BB(BasicBlock *BB); // 中插
  //...
};

class Module
{
public:
  // Module包含Function
  using FunctionPtr = std::unique_ptr<Function>;
  std::vector<FunctionPtr> &GetFuncTion();
  Module() = default;
  void push_func(Function *func);
  Function *GetMain();
};