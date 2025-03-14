#pragma once
#include "../include/lib/CoreClass.hpp"
#include "../include/lib/CFG.hpp"
#include <variant>
#include <unordered_set>
#include "CoreClass.hpp"

// Use
Use::Use(User *_user, Value *_usee) : user(_user), usee(_usee)
{
  usee->add_use(this);
}

void Use::RemoveFromValUseList(User *_user)
{
  assert(_user == user); // 必须是User
  usee->GetValUseList().GetSize()--;
  if (*prev != nullptr)
    *prev = next;
  if (next != nullptr)
    next->prev = prev;
  if (usee->GetValUseList().GetSize() == 0 && next != nullptr)
    assert(0);
  user = nullptr;
  usee = nullptr;
  prev = nullptr;
  next = nullptr;
}

// ValUseList
//  iterator 实现
ValUseList::iterator::iterator(Use *_cur) : cur(_cur) {}

ValUseList::iterator &ValUseList::iterator::operator++()
{
  cur = cur->next;
  return *this;
}

Use *ValUseList::iterator::operator*()
{
  return cur;
}

bool ValUseList::iterator::operator==(const iterator &other) const
{
  return cur == other.cur;
}

bool ValUseList::iterator::operator!=(const iterator &other) const
{
  return cur != other.cur;
}

// ValUseList 实现
void ValUseList::push_front(Use *_use)
{
  assert(_use != nullptr);
  size++;
  _use->next = head;
  if (head)
    head->prev = &(_use->next);
  _use->prev = &head;
  head = _use;
}

bool ValUseList::is_empty()
{
  return head == nullptr;
}

Use *&ValUseList::front()
{
  return head;
}

Use *ValUseList::back()
{
  if (is_empty())
    return nullptr;
  Use *current = head;
  while (current->next != nullptr)
    current = current->next;
  return current;
}

int &ValUseList::GetSize()
{
  return size;
}

ValUseList::iterator ValUseList::begin() const
{
  return iterator(this->head);
}

ValUseList::iterator ValUseList::end() const
{
  return iterator(nullptr);
}

void ValUseList::print() const
{
  std::cout << "ValUseList: size = " << size << "\n";
  for (auto it = begin(); it != end(); ++it)
  {
    std::cout << "Use: " << *it << "\n";
  }
}

void ValUseList::clear()
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

// Value
bool Value::isGlobal() { return false; }
bool Value::isConst() { return false; }

Value::Value(Type *_type) : type(_type), version(0)
{
  name = ".";
  name += std::to_string(Singleton<Module>().IR_number("."));
}

Value::Value(Type *_type, const std::string &_name) : type(_type), name(_name), version(0) {}

Value::~Value()
{
  while (!valuselist.is_empty())
    delete valuselist.front()->user;
}

Type *Value::GetType() const { return type; }

IR_DataType Value::GetTypeEnum() const { return GetType()->GetTypeEnum(); }

const std::string &Value::GetName() const { return name; }

void Value::SetName(const std::string &_name) { this->name = _name; }

void Value::SetType(Type *_type) { type = _type; }

ValUseList &Value::GetValUseList() { return valuselist; }

int Value::GetValUseListSize() { return valuselist.GetSize(); }

void Value::SetVersion(int new_version) { version = new_version; }

int Value::GetVersion() const { return version; }

Value *Value::clone(std::unordered_map<Value *, Value *> &mapping)
{
  if (isGlobal())
    return this;
  if (mapping.find(this) != mapping.end())
    return mapping[this];
  Value *newval = new Value(this->GetType());
  mapping[this] = newval;
  return newval;
}

void Value::print()
{
  if (isConst())
    std::cout << GetName();
  else if (isGlobal())
    std::cout << "@" << GetName();
  else if (auto temp = dynamic_cast<Function *>(this))
    std::cout << "@" << temp->GetName();
  // else if (auto temp = dynamic_cast<BuiltinFunc *>(this))
  //   std::cout << "@" << temp->GetName();
  else if (GetName() == "undef")
    std::cout << GetName();
  else
    std::cout << "%" << GetName();
}

void Value::add_use(Use *_use) { valuselist.push_front(_use); }

bool Value::isConstZero()
{
  if (auto num = dynamic_cast<ConstIRBoolean *>(this))
    return num->GetVal() == false;
  else if (auto num = dynamic_cast<ConstIRInt *>(this))
    return num->GetVal() == 0;
  else if (auto num = dynamic_cast<ConstIRFloat *>(this))
    return num->GetVal() == 0;
  else
    return false;
}

// User
Value *User::GetDef() { return dynamic_cast<Value *>(this); }

int User::GetUseIndex(Use *_use)
{
  assert(_use != nullptr && "Use pointer cannot be null");
  for (int i = 0; i < useruselist.size(); i++)
  {
    if (useruselist[i].get() == _use)
      return i;
  }
  assert(0);
}

void User::add_use(Value *_value)
{
  // assert(_value != nullptr && "Value pointer cannot be null");
  // for (const auto &use : useruselist)
  // {
  //   if (use->GetValue() == _value)
  //   {
  //     return;
  //   }
  // }
  useruselist.push_back(std::make_unique<Use>(this, _value));
}

bool User::remove_use(Use *_use)
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

void User::clear_use() { useruselist.clear(); }

Value *User::clone(std::unordered_map<Value *, Value *> &mapping)
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

bool User::is_empty() const { return useruselist.empty(); }

size_t User::GetUserUseListSize() const { return useruselist.size(); }

// Instruction

// Instruction::Instruction() : id(Op::None) {}

bool Instruction::IsTerminateInst() const
{
  return id == Op::UnCond || id == Op::Cond || id == Op::Ret;
}

bool Instruction::IsBinary() const
{
  return id >= Op::Add && id <= Op::G;
}

bool Instruction::IsMemoryInst() const
{
  return id >= Op::Alloca && id <= Op::Memcpy;
}

bool Instruction::IsCastInst() const
{
  return id >= Op::Zext && id <= Op::SI2FP;
}

void Instruction::add_use(Value *_value)
{
  assert(_value && "Cannot add a null operand!");
  User::add_use(_value);
}

void Instruction::print()
{
  std::cout << "Instruction: " << OpToString(id) << std::endl;
}

Value *Instruction::clone(std::unordered_map<Value *, Value *> &mapping)
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

bool Instruction::operator==(Instruction &other)
{
  return id == other.id && GetUserUseList().size() == other.GetUserUseList().size();
}

inline Operand Instruction::GetOperand(size_t idx)
{
  assert(idx < GetUserUseList().size() && "Operand index out of range!");
  return GetUserUseList()[idx]->GetValue();
}

// 给print用
const char *Instruction::OpToString(Op op)
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

// BB
std::vector<BasicBlock::InstPtr> &BasicBlock::GetInsts()
{
  return instructions;
}

BasicBlock::BasicBlock()
    : Value(VoidType::NewVoidTypeGet()), LoopDepth(0), visited(false), index(0), reachable(false)
{
}

BasicBlock::~BasicBlock() = default;

void BasicBlock::init_Insts()
{
  instructions.clear();
  NextBlocks.clear();
  PredBlocks.clear();
}

BasicBlock *BasicBlock::clone(std::unordered_map<Operand, Operand> &mapping)
{
  if (mapping.find(this) != mapping.end())
    return dynamic_cast<BasicBlock *>(mapping[this]);
  auto temp = new BasicBlock();
  mapping[this] = temp;
  for (auto i : (*this))
    temp->push_back(dynamic_cast<Instruction *>(i->clone(mapping)));
  return temp;
}

void BasicBlock::print()
{
  std::cout << "BasicBlock " << GetName() << " (Index: " << index << ", LoopDepth: " << LoopDepth << "):\n";
  for (const auto &inst : instructions)
  {
    std::cout << "  ";
    inst->print();
  }
}

std::vector<BasicBlock *> BasicBlock::GetNextBlocks() const
{
  return NextBlocks;
}

const std::vector<BasicBlock *> &BasicBlock::GetPredBlocks() const
{
  return PredBlocks;
}

void BasicBlock::AddNextBlock(BasicBlock *block)
{
  NextBlocks.push_back(block);
}

void BasicBlock::AddPredBlock(BasicBlock *pre)
{
  PredBlocks.push_back(pre);
}

void BasicBlock::RemovePredBlock(BasicBlock *pre)
{
  PredBlocks.erase(
      std::remove(PredBlocks.begin(), PredBlocks.end(), pre),
      PredBlocks.end());
}

bool BasicBlock::is_empty_Insts() const
{
  return instructions.empty();
}

Instruction *BasicBlock::GetLastInsts() const
{
  return is_empty_Insts() ? nullptr : instructions.back().get();
}

void BasicBlock::ReplaceNextBlock(BasicBlock *oldBlock, BasicBlock *newBlock)
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

void BasicBlock::ReplacePreBlock(BasicBlock *oldBlock, BasicBlock *newBlock)
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

template <typename A, typename B>
std::variant<float, int> calc(A a, BinaryInst::Operation op, B b)
{
  switch (op)
  {
  case BinaryInst::Op_Add:
    return a + b;
  case BinaryInst::Op_Sub:
    return a - b;
  case BinaryInst::Op_Mul:
    return a * b;
  case BinaryInst::Op_Div:
    return a / b;
  case BinaryInst::Op_And:
    return (a != 0) && (b != 0);
  case BinaryInst::Op_Or:
    return (a != 0) || (b != 0);
  case BinaryInst::Op_Mod:
    return (int)a % (int)b;
  case BinaryInst::Op_E:
    return a == b;
  case BinaryInst::Op_NE:
    return a != b;
  case BinaryInst::Op_G:
    return a > b;
  case BinaryInst::Op_GE:
    return a >= b;
  case BinaryInst::Op_L:
    return a < b;
  case BinaryInst::Op_LE:
    return a <= b;
  default:
    assert(0);
    break;
  }
}

bool isBinaryBool(BinaryInst::Operation _op)
{
  switch (_op)
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

Operand BasicBlock::GenerateBinaryInst(Operand _A, BinaryInst::Operation op, Operand _B)
{
  bool tpA = (_A->GetTypeEnum() == IR_DataType::IR_Value_INT);
  bool tpB = (_B->GetTypeEnum() == IR_DataType::IR_Value_INT);
  BinaryInst *tmp;

  if (tpA != tpB)
  {
    assert(op != BinaryInst::Op_And && op != BinaryInst::Op_Or && op != BinaryInst::Op_Mod);
    if (tpA)
    {
      tmp = new BinaryInst(GenerateSI2FPInst(_A), op, _B);
    }
    else
    {
      tmp = new BinaryInst(_A, op, GenerateSI2FPInst(_B));
    }
  }
  else
  {
    if (_A->GetTypeEnum() == IR_Value_INT)
    {
      bool isBooleanA = (_A->GetType() == IntType::NewIntTypeGet());
      bool isBooleanB = (_B->GetType() == IntType::NewIntTypeGet());

      if (isBooleanA != isBooleanB)
      {
        if (!isBooleanA)
        {
          _A = GenerateZextInst(_A);
        }
        else
        {
          _B = GenerateZextInst(_B);
        }
      }
    }
    tmp = new BinaryInst(_A, op, _B);
  }

  push_back(tmp);
  return Operand(tmp->GetDef());
}

Operand BasicBlock::GenerateBinaryInst(BasicBlock *bb, Operand _A, BinaryInst::Operation op, Operand _B)
{
  if (_A->isConst() && _B->isConst())
  {
    if (op == BinaryInst::Op_Div && _B->isConstZero())
    {
      assert(_A->GetType() != BoolType::NewBoolTypeGet() &&
             _B->GetType() != BoolType::NewBoolTypeGet() && "InvalidType");
      return (_A->GetType() == IntType::NewIntTypeGet())
                 ? UndefValue::Get(IntType::NewIntTypeGet())
                 : UndefValue::Get(FloatType::NewFloatTypeGet());
    }

    std::variant<float, int> result;
    if (auto A = dynamic_cast<ConstIRInt *>(_A))
    {
      if (auto B = dynamic_cast<ConstIRInt *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
      else if (auto B = dynamic_cast<ConstIRFloat *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
      else if (auto B = dynamic_cast<ConstIRBoolean *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
    }
    else if (auto A = dynamic_cast<ConstIRFloat *>(_A))
    {
      if (auto B = dynamic_cast<ConstIRInt *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
      else if (auto B = dynamic_cast<ConstIRFloat *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
      else if (auto B = dynamic_cast<ConstIRBoolean *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
    }
    else if (auto A = dynamic_cast<ConstIRBoolean *>(_A))
    {
      if (auto B = dynamic_cast<ConstIRInt *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
      else if (auto B = dynamic_cast<ConstIRFloat *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
      else if (auto B = dynamic_cast<ConstIRBoolean *>(_B))
        result = calc(A->GetVal(), op, B->GetVal());
    }

    if (isBinaryBool(op))
      return ConstIRBoolean::GetNewConstant(std::get<int>(result));
    else if (std::holds_alternative<int>(result))
      return ConstIRInt::GetNewConstant(std::get<int>(result));
    else
      return ConstIRFloat::GetNewConstant(std::get<float>(result));
  }
  else
  {
    assert(bb != nullptr);
    return bb->GenerateBinaryInst(_A, op, _B);
  }
}

Operand BasicBlock::GenerateSI2FPInst(Operand _A)
{
  auto temp = new SI2FPInst(_A);
  push_back(temp);
  return Operand(temp->GetDef());
}

Operand BasicBlock::GenerateFP2SIInst(Operand _A)
{
  auto temp = new FP2SIInst(_A);
  push_back(temp);
  return Operand(temp->GetDef());
}

Operand BasicBlock::GenerateLoadInst(Operand data)
{
  auto tmp = new LoadInst(data);
  push_back(tmp);
  return tmp->GetDef();
}

void BasicBlock::GenerateStoreInst(Operand src, Operand des)
{
  assert(des->GetType()->GetTypeEnum() == IR_PTR);
  auto tmp = static_cast<PointerType *>(des->GetType());

  if (tmp->GetSubType()->GetTypeEnum() != src->GetTypeEnum())
  {
    src = (tmp->GetSubType()->GetTypeEnum() == IR_Value_INT) ? this->GenerateFP2SIInst(src) : this->GenerateSI2FPInst(src);
  }

  auto storeinst = std::make_unique<StoreInst>(src, des);
  this->push_back(storeinst.get());
  instructions.emplace_back(std::move(storeinst));
}

AllocaInst *BasicBlock::GenerateAlloca(Type *_tp, std::string name)
{
  auto tp = PointerType::NewPointerTypeGet(_tp);
  auto alloca = new AllocaInst(tp);
  Singleton<Module>().Register(name, alloca);
  GetParent()->GetFront()->push_front(alloca);
  return alloca;
}

void BasicBlock::GenerateCondInst(Operand _cond, BasicBlock *_true,
                                  BasicBlock *_false)
{
  auto condinst = new CondInst(_cond, _true, _false);
  push_back(condinst);
}
void BasicBlock::GenerateUnCondInst(BasicBlock *des)
{
  auto uncondinst = new UnCondInst(des);
  push_back(uncondinst);
}

// 根据name生成 CallInst
Operand BasicBlock::GenerateCallInst(std::string id, std::vector<Operand> args, int run_time)
{
  static const std::unordered_set<std::string> builtin_functions = {
      "getint", "getfloat", "getch", "getarray", "getfarray", "putint",
      "putch", "putarray", "putfloat", "putfarray", "starttime", "stoptime",
      "putf", "llvm.memcpy.p0.p0.i32"};

  if (builtin_functions.count(id))
  {
    if (id == "starttime" || id == "stoptime")
    {
      assert(args.empty());
      args.push_back(ConstIRInt::GetNewConstant(run_time));
    }

    if (id == "putfloat")
    {
      if (args[0]->GetTypeEnum() == IR_Value_INT)
      {
        args[0] = GenerateSI2FPInst(args[0]);
      }
    }
    else if (id == "putint" || id == "putch" || id == "putarray" || id == "putfarray")
    {
      if (args[0]->GetTypeEnum() == IR_Value_Float)
      {
        args[0] = GenerateFP2SIInst(args[0]);
      }
    }

    auto call_inst = std::make_unique<CallInst>(BuiltinFunc::GetBuiltinFunc(id), args, "at" + std::to_string(run_time));
    push_back(call_inst.get());
    instructions.emplace_back(std::move(call_inst));
    return instructions.back()->GetDef();
  }

  if (auto func = static_cast<Function *>(Singleton<Module>().GetValueByName(id)))
  {
    auto &params = func->GetParams();
    assert(args.size() == params.size());

    auto arg_iter = args.begin();
    for (auto &param : params)
    {
      auto &arg = *arg_iter;
      auto param_type = param->GetType();

      if (param_type != arg->GetType())
      {
        auto arg_enum = arg->GetType()->GetTypeEnum();
        auto param_enum = param_type->GetTypeEnum();
        assert(arg_enum == IR_Value_INT || arg_enum == IR_Value_Float);
        assert(param_enum == IR_Value_INT || param_enum == IR_Value_Float);

        arg = (param_enum == IR_Value_Float) ? GenerateSI2FPInst(arg) : GenerateFP2SIInst(arg);
      }
      ++arg_iter;
    }

    auto call_inst = std::make_unique<CallInst>(func, args, "at" + std::to_string(run_time));
    push_back(call_inst.get());
    instructions.emplace_back(std::move(call_inst)); // 确保生命周期
    return instructions.back()->GetDef();
  }

  std::cerr << "No Such Function!\n";
  assert(0);
  return nullptr;
}

void BasicBlock::GenerateRetInst()
{
  auto retinst = new RetInst();
  push_back(retinst);
}

void BasicBlock::GenerateRetInst(Operand ret_val)
{
  if (GetParent()->GetTypeEnum() != ret_val->GetTypeEnum())
  {
    if (ret_val->GetTypeEnum() == IR_Value_INT)
      ret_val = GenerateSI2FPInst(ret_val);
    else
      ret_val = GenerateFP2SIInst(ret_val);
  }
  auto retinst = new RetInst(ret_val);
  push_back(retinst);
}

Operand BasicBlock::GenerateGepInst(Operand ptr)
{
  auto tmp = new GepInst(ptr);
  push_back(tmp);
  return tmp->GetDef();
}

Operand BasicBlock::GenerateZextInst(Operand ptr)
{
  auto tmp = new ZextInst(ptr);
  push_back(tmp);
  return tmp->GetDef();
}

BasicBlock *BasicBlock::GenerateNewBlock()
{
  BasicBlock *tmp = new BasicBlock();
  GetParent()->push_back(tmp);
  return tmp;
}
// push_back存疑
BasicBlock *BasicBlock::GenerateNewBlock(std::string name)
{
  BasicBlock *tmp = new BasicBlock();
  tmp->name += name;
  GetParent()->push_back(tmp);
  return tmp;
}

// Function
Function::Function(IR_DataType _type, const std::string &_id)
    : Value(NewTypeFromIRDataType(_type)), id(_id)
{
  // 构造默认vector和mylist双开
  push_back(new BasicBlock());
}

std::vector<Function::ParamPtr> &Function::GetParams()
{
  return params;
}

std::vector<Function::BBPtr> &Function::GetBBs()
{
  return BBs;
}

auto Function::begin()
{
  return BBs.begin();
}

auto Function::end()
{
  return BBs.end();
}

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

Function *Function::clone(std::unordered_map<Value *, Value *> &)
{
  return this;
}

void Function::AddBBs(BasicBlock *BB)
{
  BBs.push_back(std::unique_ptr<BasicBlock>(BB));
  size_BB++;
}

void Function::PushBothBB(BasicBlock *BB)
{
  AddBBs(BB);
  push_back(BB);
}

void Function::InsertBBs(BasicBlock *BB, size_t pos)
{
  if (pos > BBs.size())
  {
    pos = BBs.size(); // 防越界
  }
  BBs.insert(BBs.begin() + pos, std::unique_ptr<BasicBlock>(BB));
  size_BB++;
}

void Function::InsertBB(BasicBlock *pred, BasicBlock *succ, BasicBlock *insert)
{
}

void Function::InsertBB(BasicBlock *curr, BasicBlock *insert)
{
}

void Function::RemoveBBs(BasicBlock *BB)
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

void Function::InitBBs()
{
  BBs.clear();
  size_BB = 0;
}

void Function::PushParam(std::string name, Var *var)
{
  auto alloca = new AllocaInst(PointerType::NewPointerTypeGet(var->GetType()));
  auto store = new StoreInst(var, alloca);
  GetFront()->push_front(alloca);
  GetFront()->push_back(store);
  Singleton<Module>().Register(name, alloca);
  params.emplace_back(var);
}

// 如果后面出错，可能问题在这儿
std::vector<BasicBlock *> Function::GetRetBlock()
{
  std::vector<BasicBlock *> tmp;
  for (auto &BB : *this)
  {
    auto *inst = BB->GetBack();
    if (auto *retInst = dynamic_cast<RetInst *>(inst))
    {
      tmp.push_back(BB.get());
    }
  }
  return tmp;
}

// Module
void Module::push_func(FunctionPtr func)
{
  if (func)
    functions.push_back(std::move(func));
}

Function *Module::GetMain()
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

void Module::PushVar(Var *ptr)
{
  assert(ptr->usage != Var::Param && "Wrong API Usage");
  globalvaribleptr.emplace_back(ptr);
}

std::string Module::GetFuncNameEnum(std::string name)
{
  // 使用unordered_set提前存储所有函数名称，加速查找
  std::unordered_set<std::string> existingNames;
  for (const auto &func : functions)
  {
    existingNames.insert(func->GetName());
  }

  int i = 0;
  while (true)
  {
    std::string newName = name + "_" + std::to_string(i);
    if (existingNames.find(newName) == existingNames.end())
    {
      return newName;
    }
    i++;
  }
}

Function &Module::GenerateFunction(IR_DataType _tp, std::string _id)
{
  auto tmp = new Function(_tp, _id);
  Register(_id, tmp);
  functions.push_back(FunctionPtr(tmp));
  return *functions.back();
}
