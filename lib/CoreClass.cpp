#include"../include/lib/CoreClass.hpp"

//Use
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

//ValUseList
// iterator 实现
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

//Value
bool Value::isGlobal() { return false; }
bool Value::isConst() { return false; }

Value::Value(Type *_type) : type(_type), version(0)
{
    name = "t";
    // name += std::to_string(Singleton<Module>().IR_number("."));
    // 结果：t1,t2,t3
    // 单例
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
    // else if (auto temp = dynamic_cast<BuildInFunction *>(this))
    //   std::cout << "@" << temp->GetName();
    else if (GetName() == "undef")
        std::cout << GetName();
    else
        std::cout << "%" << GetName();
}

void Value::add_use(Use *_use) { valuselist.push_front(_use); }


//User
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
    assert(_value != nullptr && "Value pointer cannot be null");
    for (const auto &use : useruselist)
    {
        if (use->GetValue() == _value)
        {
            return; // 如果存在，直接返回
        }
    }
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

//Instruction

//Instruction::Instruction() : id(Op::None) {}

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

//BB
std::vector<BasicBlock::InstPtr> &BasicBlock::GetInsts()
{
  return instructions;
}

BasicBlock::BasicBlock() 
  : Value(VoidType::NewVoidTypeGet()), LoopDepth(0), visited(false), index(0), reachable(false) 
{}

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

Operand BasicBlock::GenerateBinaryInst(BasicBlock *_BB, Operand _A,
    BinaryInst::Operation _op, Operand _B){
        //...
}

//Function
Function::Function(IR_DataType _type, const std::string &_id)
    : Value(NewTypeFromIRDataType(_type)), id(_id)
{
    // 构造默认vector和mylist双开
    push_back(new BasicBlock());
}

std::vector<Function::ParamPtr> &Function::GetParams() {
    return params;
}

std::vector<Function::BBPtr> &Function::GetBBs() {
    return BBs;
}

auto Function::begin() {
    return BBs.begin();
}

auto Function::end() {
    return BBs.end();
}

void Function::print() {
    std::cout << "define ";
    type->print();
    std::cout << " @" << name << "(";
    for (auto &i : params) {
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

Function *Function::clone(std::unordered_map<Value *, Value *> &) {
    return this;
}

void Function::add_BBs(BasicBlock *BB) {
    BBs.push_back(std::unique_ptr<BasicBlock>(BB));
    size_BB++;
}

void Function::push_both_BB(BasicBlock *BB) {
    add_BBs(BB);
    push_back(BB);
}

void Function::Insert_BBs(BasicBlock *BB, size_t pos) {
    if (pos > BBs.size()) {
        pos = BBs.size(); // 防越界
    }
    BBs.insert(BBs.begin() + pos, std::unique_ptr<BasicBlock>(BB));
    size_BB++;
}

void Function::Insert_BB(BasicBlock *pred, BasicBlock *succ, BasicBlock *insert) {

}

void Function::Insert_BB(BasicBlock *curr, BasicBlock *insert) {

}

void Function::Remove_BBs(BasicBlock *BB) {
    auto it = std::find_if(BBs.begin(), BBs.end(),
                           [BB](const BBPtr &ptr) { return ptr.get() == BB; });
    if (it != BBs.end()) {
        BBs.erase(it);
        size_BB--;
    }
}

void Function::init_BBs() {
    BBs.clear();
    size_BB = 0;
}

void Function::push_param() {

}

std::vector<BasicBlock *> Function::GetRetBlock() {

}

//Module
std::vector<std::unique_ptr<Function>> &Module::GetFuncTion() {
  return functions;
}

void Module::push_func(FunctionPtr func) {
  if (func)
      functions.push_back(std::move(func));
}

Function *Module::GetMain() {
  for (auto &func : functions) {
      if (func && func->GetName() == "main") {
          return func.get();
      }
  }
  return nullptr;
}