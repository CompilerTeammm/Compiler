#include "../include/lib/CoreClass.hpp"
#include "../include/lib/CFG.hpp"
#include <cassert>
#include <sstream>

// Use
Use::Use(User *_user, Value *_usee) : user(_user), usee(_usee)
{
  usee->add_use(this);
}

void Use::RemoveFromValUseList(User *_user)
{
  assert(_user == user); // 必须是User
  if (usee)
    usee->GetValUseList().GetSize()--;
  if (*prev != nullptr)
    *prev = next;
  if (next != nullptr)
    next->prev = prev;
  if (usee)
    if (usee->GetValUseList().GetSize() == 0 && next != nullptr)
      assert(0);

  // all is nullptr cant't fullfill the operator++
  // user = nullptr;
  // usee = nullptr;
  // prev = nullptr;
  // next = nullptr;
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
  // return head == nullptr;
  return size == 0;
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

Type *Value::GetType() { return type; }

IR_DataType Value::GetTypeEnum() { return GetType()->GetTypeEnum(); }

std::string Value::GetName() { return name; }

void Value::SetName(std::string _name) { this->name = _name; }

void Value::SetType(Type *_type) { type = _type; }

ValUseList &Value::GetValUseList() { return valuselist; }

int Value::GetValUseListSize() { return valuselist.GetSize(); }

// 做两件事情，消除原来的Use*的关系
// 对于value需要它去继承之前的关系
// phi 的处理貌似没有做, sccp 的时候如果不做phi函数的处理会报错
void Value::ReplaceAllUseWith(Value *value) // dh RAUW
{
  // auto list =  valuselist;
  valuselist.GetSize() = 0;

  Use *&Head = valuselist.front();
  while (Head)
  {
    // PhiInst 需要专门去传播的原因是：它是在Incoming中使用的
    // 这里面的替换都是在Inst中去替换
    if (PhiInst *PInst = dynamic_cast<PhiInst *>(Head->GetUser()))
      PInst->PhiProp(Head->GetValue(), value);
    Head->usee = value;
    Use *tmp = Head->next;
    value->valuselist.push_front(Head);
    Head = tmp;
  }
}

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
  else if (auto temp = dynamic_cast<BuiltinFunc *>(this))
    std::cout << "@" << temp->GetName();
  else if (GetName() == "undef")
    std::cout << GetName();
  else
    std::cout << "%" << GetName();
}

void Value::add_use(Use *_use) { valuselist.push_front(_use); }

bool Value::IsUndefVal()
{
  if (dynamic_cast<UndefValue *>(this))
    return true;
  else
    return false;
}

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

bool Value::isConstOne()
{
  if (auto num = dynamic_cast<ConstIRInt *>(this))
    return num->GetVal() == 1;
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

User *User::CloneInst()
{
  if (auto call = dynamic_cast<CallInst *>(this))
  {
    std::vector<Value *> tmp;
    std::for_each(call->GetUserUseList().begin() + 1, call->GetUserUseList().end(),
                  [&tmp](auto &ele)
                  { tmp.push_back(ele->GetValue()); });
    return new CallInst(call->GetUserUseList()[0]->GetValue(), tmp);
  }
  else if (auto bin = dynamic_cast<BinaryInst *>(this))
  {
    return new BinaryInst(bin->GetOperand(0), bin->GetOp(),
                          bin->GetOperand(1));
  }
  else if (auto gep = dynamic_cast<GepInst *>(this))
  {
    std::vector<Value *> tmp;
    for (int i = 1; i < gep->GetUserUseListSize(); i++)
    {
      tmp.push_back(gep->GetOperand(i));
    }
    return new GepInst(gep->GetOperand(0), tmp);
  }
  else if (auto ld = dynamic_cast<LoadInst *>(this))
  {
    return new LoadInst(ld->GetOperand(0));
  }
  else if (auto st = dynamic_cast<StoreInst *>(this))
  {
    return new StoreInst(st->GetOperand(0), st->GetOperand(1));
  }
  else if (auto cond = dynamic_cast<CondInst *>(this))
  {
    return new CondInst(cond->GetOperand(0),
                        dynamic_cast<BasicBlock *>(cond->GetOperand(1)),
                        dynamic_cast<BasicBlock *>(cond->GetOperand(2)));
  }
  else if (auto uncond = dynamic_cast<UnCondInst *>(this))
  {
    return new UnCondInst(dynamic_cast<BasicBlock *>(uncond->GetOperand(0)));
  }
  else if (auto phi = dynamic_cast<PhiInst *>(this))
  {
    auto new_phi = PhiInst::Create(phi->GetType());
    for (const auto &[index, val] : phi->PhiRecord)
    {
      phi->addIncoming(val.first, val.second);
    }
    return new_phi;
  }
  else if (auto max = dynamic_cast<MaxInst *>(this))
    return new MaxInst(GetOperand(0), GetOperand(1));
  else if (auto min = dynamic_cast<MinInst *>(this))
    return new MinInst(GetOperand(0), GetOperand(1));
  else if (auto select = dynamic_cast<SelectInst *>(this))
    return new SelectInst(GetOperand(0), GetOperand(1), GetOperand(2));
  assert(0);
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

bool Instruction::IsCallInst() const
{
  return id == Op::Call;
}

bool Instruction::IsGepInst() const
{
  return id == Op::Gep;
}

bool Instruction::IsMinInst() const
{
  return id == Op::Min;
}

bool Instruction::IsMaxInst() const
{
  return id == Op::Max;
}

bool Instruction::IsSelectInst() const
{
  return id == Op::Select;
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

// inline Operand Instruction::GetOperand(size_t idx)
// {
//   assert(idx < GetUserUseList().size() && "Operand index out of range!");
//   return GetUserUseList()[idx]->GetValue();
// }

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

void Instruction::InstReplace(Instruction *inst)
{
  assert(inst->GetType() == this->GetType() && "Type not match!");
  ReplaceAllUseWith(inst);
  ReplaceNode(inst);
}

ConstantData::ConstantData(Type *_tp) : Value(_tp) {}

ConstantData *ConstantData::getNullValue(Type *tp)
{
  IR_DataType type = tp->GetTypeEnum();
  if (type == IR_DataType::IR_Value_INT)
    return ConstIRInt::GetNewConstant(0);
  else if (type == IR_DataType::IR_Value_Float)
    return ConstIRFloat::GetNewConstant(0);
  else
    return nullptr;
}

bool ConstantData::isConst()
{
  return true;
}

bool ConstantData::isZero()
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

ConstIRBoolean::ConstIRBoolean(bool _val)
    : ConstantData(BoolType::NewBoolTypeGet()), val(_val)
{
  if (val)
    name = "true";
  else
    name = "false";
}

ConstIRBoolean *ConstIRBoolean::GetNewConstant(bool _val)
{
  static ConstIRBoolean true_const(true);
  static ConstIRBoolean false_const(false);
  if (_val)
    return &true_const;
  else
    return &false_const;
}

bool ConstIRBoolean::GetVal()
{
  return val;
}

ConstIRInt::ConstIRInt(int _val)
    : ConstantData(IntType::NewIntTypeGet()), val(_val)
{
  name = std::to_string(val);
}

ConstIRInt *ConstIRInt::GetNewConstant(int _val)
{
  static std::map<int, ConstIRInt *> int_const_map;
  if (int_const_map.find(_val) == int_const_map.end())
    int_const_map[_val] = new ConstIRInt(_val);
  return int_const_map[_val];
}

int ConstIRInt::GetVal()
{
  return val;
}

ConstIRFloat::ConstIRFloat(float _val)
    : ConstantData(FloatType::NewFloatTypeGet()), val(_val)
{
  // 使用 double 更精确地处理浮点数的二进制表示
  double tmp = val;
  std::stringstream hexStream;
  hexStream << "0x" << std::hex << *(reinterpret_cast<long long *>(&tmp)); // 十六进制表示
  name = hexStream.str();
}

ConstIRFloat *ConstIRFloat::GetNewConstant(float _val)
{
  static std::map<float, ConstIRFloat *> float_const_map;
  if (float_const_map.find(_val) == float_const_map.end())
    float_const_map[_val] = new ConstIRFloat(_val);
  return float_const_map[_val];
}

float ConstIRFloat::GetVal()
{
  return val;
}

double ConstIRFloat::GetValAsDouble() const
{
  return static_cast<double>(val);
}