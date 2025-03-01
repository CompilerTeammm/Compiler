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
#include "CFG.hpp"

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
private:
  int size = 0;
  Use *head = nullptr;

public:
  // 默认构造
  ValUseList() = default;

  // 遍历UseList的Use
  class iterator
  {
  private:
    // 用于存储当前元素的指针cur
    Use *cur;

  public:
    explicit iterator(Use *_cur);

    iterator &operator++();
    // 操作符重载：*、==、！=
    // 解引用*,直接访问Use而不是Use*
    Use *operator*();
    //==
    bool operator==(const iterator &other) const;
    // ！=
    bool operator!=(const iterator &other) const;
  };

  // 给该User添加Use
  void push_front(Use *_use);

  // ValUseList常用操作：
  // 判空
  bool is_empty();
  // 获取头尾
  Use *&front();
  Use *back();
  // 获取长度
  int &GetSize();
  // 头尾
  iterator begin() const;
  iterator end() const;
  // eg:for (auto it = list.begin(); it != list.end(); ++it)

  void print() const; // 调试
  void clear();
};


class Value
{
private:
  friend class Module;
  // (Value) this is the key to find Users 
  ValUseList valuselist;
protected:
  std::string name;
  Type *type;
  int version;

public:
  virtual bool isGlobal();
  virtual bool isConst();

  // 构造至少需要类型，可以不要value
  Value() = delete;
  Value(Type *_type);
  Value(Type *_type, const std::string &_name);
  virtual ~Value();

  // 基本操作：获取&设置各种值
  virtual Type *GetType() const;
  IR_DataType GetTypeEnum() const;
  const std::string &GetName() const;
  void SetName(const std::string &_name);
  void SetType(Type *_type);
  ValUseList &GetValUseList();
  int GetValUseListSize();
  void SetVersion(int new_version);
  int GetVersion() const;

  // 克隆，以Value*形式返回
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping);

  void print();
  void add_use(Use *_use);
};

class User : public Value
{
private:
  using UsePtr = std::unique_ptr<Use>;
  using UseList = std::vector<UsePtr>;
protected:
  UseList useruselist;

public:
  User() = default;
  explicit User(Type *_type);
  virtual ~User() = default;

  // 将User的指针转换为 Value *
  virtual Value *GetDef();

  // 获取Use的序号
  int GetUseIndex(Use *_use);

  UseList &GetUserUseList(){return this->useruselist;}

  virtual void add_use(Value *_value);
  bool remove_use(Use *_use);
  void clear_use();

  // 默认调用Value的print
  virtual void print() = 0;

  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping) override;

  bool is_empty() const;
  size_t GetUserUseListSize() const;
};

class Instruction : public User,public Node<BasicBlock,Instruction>
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
  bool IsTerminateInst() const;
  // 判断是否为二元操作符
  bool IsBinary() const;
  // 是否为内存相关指令
  bool IsMemoryInst() const;
  // 是否为类型转换指令
  bool IsCastInst() const;

  void add_use(Value *_value) override;
  virtual void print() override;
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping) override;

  // 判断是否等于另一条指令
  bool operator==(Instruction &other);
  // 获取指定索引的操作数  User 获取 value的一个接口
  Value *GetOperand(size_t idx);
  // 将指令类型转换为字符串,便于调试
  static const char *OpToString(Op op);
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
  //原来是public
private:
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

public:
  // 获取当前基本块的指令
  std::vector<InstPtr> &GetInsts();

  BasicBlock(); // 构造函数
  virtual ~BasicBlock(); // 析构函数

  virtual void init_Insts(); // 初始化指令

  // 复制mylist
  BasicBlock *clone(std::unordered_map<Operand, Operand> &mapping) override;

  virtual void print();

  // 获取后继基本块列表
  std::vector<BasicBlock *> GetNextBlocks() const;

  // 获取前驱基本块
  const std::vector<BasicBlock *> &GetPredBlocks() const;

  // 添加后继基本块
  void AddNextBlock(BasicBlock *block);

  // 添加前驱基本块
  void AddPredBlock(BasicBlock *pre);

  // 移除前驱基本块
  void RemovePredBlock(BasicBlock *pre);

  bool is_empty_Insts() const; // 判断指令是否为空

  // 获取基本块的最后一条指令
  Instruction *GetLastInsts() const;

  // 替换后继块中的某个基本块
  void ReplaceNextBlock(BasicBlock *oldBlock, BasicBlock *newBlock);

  // 替换前驱块中的某个基本块
  void ReplacePreBlock(BasicBlock *oldBlock, BasicBlock *newBlock);

  // 暂未实现，只有声明
  Operand GenerateBinaryInst(BasicBlock *_BB, Operand _A,
                             BinaryInst::Operation _op, Operand _B);
};

// 既提供了vector线性管理BasicBlock，又实现了双向链表
class Function : public Value, public List<Function, BasicBlock>
{
private:
    using ParamPtr = std::unique_ptr<Value>;
    using BBPtr = std::unique_ptr<BasicBlock>;
    std::vector<ParamPtr> params;
    std::vector<BBPtr> BBs;
    std::string id;
    int size_BB = 0;

public:
    Function(IR_DataType _type, const std::string &_id);
    ~Function() = default;

    std::vector<ParamPtr> &GetParams();
    std::vector<BBPtr> &GetBBs();
    auto begin();
    auto end();
    void print();
    virtual Function *clone(std::unordered_map<Value *, Value *> &) override;

    //带BB的都是操作vector，List的相关函数在MyLsit
    void add_BBs(BasicBlock *BB);
    void push_both_BB(BasicBlock *BB);
    void Insert_BBs(BasicBlock *BB, size_t pos);
    void Insert_BB(BasicBlock *pred, BasicBlock *succ, BasicBlock *insert);
    void Insert_BB(BasicBlock *curr, BasicBlock *insert);
    void Remove_BBs(BasicBlock *BB);
    void init_BBs();
    void push_param();
    std::vector<BasicBlock *> GetRetBlock();
};

class Module
{
private:
    using FunctionPtr = std::unique_ptr<Function>;
    std::vector<FunctionPtr> functions;

public:
    Module() = default;
    ~Module() = default;

    std::vector<FunctionPtr> &GetFuncTion();

    void push_func(FunctionPtr func);

    Function *GetMain();
};