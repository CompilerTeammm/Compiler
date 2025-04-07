// #pragma once
// #include <cassert>
// #include <string>
// #include <vector>
// #include <iostream>
// #include <memory>
// #include <unordered_map>
// #include <algorithm>
// #include <set>
// #include <variant>
// #include <map>
// #include <stack>
// #include <sstream>
// #include "Type.hpp"
// #include "Singleton.hpp"
// #include "MyList.hpp"
// #include "CFG.hpp"
// #include "SymbolTable.hpp"

// class Use;
// class User;
// class Value;
// class Instruction;
// class BasicBlock;
// class Function;
// class Module;
// class BinaryInst;

// class ValUseList;

#pragma once
#include <vector>
#include <cassert>
#include "MyList.hpp"
#include <variant>
#include <functional>
#include <iostream>
#include <cxxabi.h>
#include "Type.hpp"
#include <string>
#include <set>
#include <unordered_set>
class User;
class Value;
class BasicBlock;
class Function;

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
  virtual bool IsBoolean() const { return false; }
  // 构造至少需要类型，可以不要value
  Value() = delete;
  Value(Type *_type);
  Value(Type *_type, const std::string &_name);
  virtual ~Value();

  // 基本操作：获取&设置各种值
  virtual Type *GetType();
  IR_DataType GetTypeEnum();
  virtual std::string GetName();
  void SetName(std::string _name);
  void SetType(Type *_type);
  ValUseList &GetValUseList();
  int GetValUseListSize();
  void ReplaceAllUseWith(Value *value); // dh RAUW
  void SetVersion(int new_version);
  int GetVersion() const;
  bool IsUndefVal();
  // 克隆，以Value*形式返回
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping);

  bool use_empty() { return valuselist.is_empty(); }

  void print();
  void add_use(Use *_use);
  bool isConstZero();
};

class User : public Value
{
public:
  using UsePtr = std::unique_ptr<Use>;
  using UseList = std::vector<UsePtr>;

protected:
  UseList useruselist;

public:
  User() : Value(VoidType::NewVoidTypeGet()) {}
  User(Type *_type) : Value(_type) {}
  virtual ~User() = default;

  // 将User的指针转换为 Value *
  virtual Value *GetDef();

  // 获取Use的序号
  int GetUseIndex(Use *_use);

  UseList &GetUserUseList() { return this->useruselist; }

  virtual void add_use(Value *_value);
  bool remove_use(Use *_use);
  void clear_use();

  int GetOperandNums() {  return useruselist.size(); }
  inline Operand GetOperand(int i) { return useruselist[i]->GetValue(); }
  inline void SetOperand(int i,Value* val) { useruselist[i]->usee = val; }

  // 默认调用Value的print
  virtual void print() = 0;

  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping) override;

  bool is_empty() const;
  size_t GetUserUseListSize() const;
};

class Instruction : public User, public Node<BasicBlock, Instruction>
{
public:
  enum Op
  {
    None,
    // Terminators
    UnCond,             // br label %.1
    Cond,               
    Ret,                // ret i32 1; or  ret void;
    // Memory
    Alloca,              
    Load,
    Store,
    Memcpy,             // llvm Intrinsic
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
    Gep,                // GetElementPtr
    Phi,        
    Call,               // call i32 @add(i32 , i32)
    Zext,               // 0扩展
    Sext,               // 符号扩展
    Trunc,              // 截断指令
    FP2SI,              // 浮点到有符号整数， fptosi
    SI2FP,              // 有符号整数到浮点  sitofp
    BinaryUnknown,      
    Max,
    Min,
    Select              // ? : 运算符
  };
  Op id; // 指令类型

  Instruction() : User() {}
  Instruction(Type *_type, Op _Op) : User(_type), id(_Op) {}
  Instruction(Type *_type) : User(_type) {}

  Op GetInstId() const { return id; }

  // 判断是否为结束指令
  bool IsTerminateInst() const;
  // 判断是否为二元操作符
  bool IsBinary() const;
  // 是否为内存相关指令
  bool IsMemoryInst() const;
  // 是否为类型转换指令
  bool IsCastInst() const;

  bool IsCallInst() const;

  void add_use(Value *_value) override;
  virtual void print() = 0;
  virtual Value *clone(std::unordered_map<Value *, Value *> &mapping) override;

  // 判断是否等于另一条指令
  bool operator==(Instruction &other);
  // 获取指定索引的操作数  User 获取 value的一个接口
  // Value *GetOperand(size_t idx);
  // 暂未实现：依赖RAUW->PhiInst,等中端实现了再来补充
  // void InstReplace(Instruction *inst);
  // 将指令类型转换为字符串,便于调试
  static const char *OpToString(Op op);

  virtual ~Instruction() = default;
};

// 示例子类指令，继承自Instruction,放到CFG实现
// class AllocaInst : public Instruction
// {
// }; //...

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
