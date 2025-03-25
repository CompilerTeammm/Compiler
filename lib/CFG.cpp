#include "../include/lib/CFG.hpp"
#include "../include/lib/CoreClass.hpp"
#include "../include/lib/MagicEnum.hpp"
// #include "../util/my_stl.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#define BaseEnumNum 8

template <typename T>
T *normal_clone(T *from, std::unordered_map<Operand, Operand> &mapping)
{
    if (mapping.find(from) != mapping.end())
        return dynamic_cast<T *>(mapping[from]);
    auto tmp = new T(from->GetType());
    mapping[from] = tmp;
    return dynamic_cast<T *>(from->User::clone(mapping));
}

Initializer::Initializer(Type *_tp) : Value(_tp) {}

void Initializer::Var2Store(BasicBlock *block, const std::string &name, std::vector<int> &gep_data)
{
    auto module = Singleton<Module>().GetValueByName(name);
    auto base_gep = dynamic_cast<GepInst *>(block->GenerateGepInst(module));

    for (size_t i = 0; i < this->size(); i++)
    {
        auto &handle = (*this)[i];
        gep_data.push_back(i);
        if (auto inits = dynamic_cast<Initializer *>(handle))
        {
            inits->Var2Store(block, name, gep_data);
        }
        else
        {
            if (!handle->isConst())
            {
                auto gep = base_gep;
                for (int j : gep_data)
                    gep->add_use(ConstIRInt::GetNewConstant(j));

                block->GenerateStoreInst(handle, gep);
                if (handle->GetType()->GetTypeEnum() == IR_Value_INT)
                    handle = ConstIRInt::GetNewConstant();
                else
                    handle = ConstIRFloat::GetNewConstant();
            }
        }
        gep_data.pop_back();
    }
}

Operand Initializer::GetInitVal(std::vector<int> &idx, int dep)
{
    auto basetp = dynamic_cast<HasSubType *>(GetType())->GetBaseType();
    auto getZero = [&]() -> Operand
    {
        if (basetp == IntType::NewIntTypeGet())
        {
            return ConstIRInt::GetNewConstant();
        }
        else if (basetp == FloatType::NewFloatTypeGet())
        {
            return ConstIRFloat::GetNewConstant();
        }
        else
        {
            return ConstIRBoolean::GetNewConstant();
        }
    };
    int thissize = size();
    if (thissize == 0)
    {
        return getZero();
    }

    auto arrType = dynamic_cast<ArrayType *>(type);
    if (!arrType)
    {
        return getZero();
    }

    int limi = arrType->GetNum();
    auto i = idx[dep];
    if (i >= limi)
    {
        return getZero();
    }
    if (i >= thissize)
    {
        return getZero();
    }

    auto handle = (*this)[i];
    if (auto inits = dynamic_cast<Initializer *>(handle))
    {
        return inits->GetInitVal(idx, dep + 1);
    }
    return handle;
}

Var::Var(UsageTag tag, Type *_tp, std::string _id)
    : User(tag == Param ? _tp : PointerType::NewPointerTypeGet(_tp)),
      usage(tag)
{
    if (usage == Param)
    {
        // name = _id;
        return;
    }
    if (usage == GlobalVar)
    {
        name = ".G." + _id;
        Singleton<Module>().Register(_id, this);
    }
    else if (usage == Constant)
        name = ".C." + name;
    Singleton<Module>().PushVar(this);
}

BuiltinFunc::BuiltinFunc(Type *tp, std::string _id) : Value(tp)
{
    name = _id;
    if (name == "starttime" || name == "stoptime")
        name = "_sysy_" + name;
}

bool BuiltinFunc::CheckBuiltin(std::string id)
{
    if (id == "getint")
        return true;
    if (id == "getfloat")
        return true;
    if (id == "getch")
        return true;
    if (id == "getarray")
        return true;
    if (id == "getfarray")
        return true;
    if (id == "putint")
        return true;
    if (id == "putch")
        return true;
    if (id == "putarray")
        return true;
    if (id == "putfloat")
        return true;
    if (id == "putfarray")
        return true;
    if (id == "starttime")
        return true;
    if (id == "stoptime")
        return true;
    if (id == "putf")
        return true;
    if (id == "llvm.memcpy.p0.p0.i32")
        return true;
    return false;
}

// const std::string &_id
BuiltinFunc *BuiltinFunc::GetBuiltinFunc(std::string _id)
{
    static std::unordered_map<std::string, BuiltinFunc *> mp;
    static const std::unordered_map<std::string, Type *> type_map = {
        {"getint", IntType::NewIntTypeGet()},
        {"getfloat", FloatType::NewFloatTypeGet()},
        {"getch", IntType::NewIntTypeGet()},
        {"getarray", IntType::NewIntTypeGet()},
        {"getfarray", IntType::NewIntTypeGet()},
        {"putint", VoidType::NewVoidTypeGet()},
        {"putch", VoidType::NewVoidTypeGet()},
        {"putarray", VoidType::NewVoidTypeGet()},
        {"putfloat", VoidType::NewVoidTypeGet()},
        {"putfarray", VoidType::NewVoidTypeGet()},
        {"starttime", VoidType::NewVoidTypeGet()},
        {"stoptime", VoidType::NewVoidTypeGet()},
        {"putf", VoidType::NewVoidTypeGet()},
        {"llvm.memcpy.p0.p0.i32", VoidType::NewVoidTypeGet()},
        {"memcpy@plt", VoidType::NewVoidTypeGet()},
        {"buildin_NotifyWorkerLE", VoidType::NewVoidTypeGet()},
        {"buildin_NotifyWorkerLT", VoidType::NewVoidTypeGet()},
        {"buildin_FenceArgLoaded", VoidType::NewVoidTypeGet()},
        {"buildin_AtomicF32add", VoidType::NewVoidTypeGet()},
        {"CacheLookUp", VoidType::NewVoidTypeGet()},
    };

    auto it = type_map.find(_id);
    if (it == type_map.end())
    {
        std::cerr << "Error: Unknown built-in function '" << _id << "'\n";
        assert(0);
    }

    auto [iter, inserted] = mp.try_emplace(_id, nullptr);
    if (inserted)
    {
        iter->second = new BuiltinFunc(it->second, _id);
    }
    return iter->second;
}

// CallInst *BuiltinFunc::BuiltinTransform(CallInst *callinst)
// {
//     std::string funcName = callinst->GetOperand(0)->GetName();

//     if (!CheckBuiltin(funcName))
//     {
//         return callinst;
//     }

//     if (funcName == "llvm.memcpy.p0.p0.i32")
//     {
//         auto dst = callinst->GetOperand(1);
//         auto src = callinst->GetOperand(2);
//         auto size = callinst->GetOperand(3);

//         std::vector<Operand> args{dst, src, size};

//         auto tmp = new CallInst(BuiltinFunc::GetBuiltinFunc("memcpy@plt"), args, "");

//         // 替换原来的调用
//         callinst->InstReplace(tmp);

//         delete callinst;
//         return tmp;
//     }

//     return callinst;
// }

Instruction *BuiltinFunc::GenerateCallInst(std::string id, std::vector<Operand> args)
{
    if (CheckBuiltin(id))
    {
        // 目前只支持memcpy,可拓展
        if (id != "llvm.memcpy.p0.p0.i32")
        {
            throw std::runtime_error("Builtin function '" + id + "' is not supported here.");
        }
        return new CallInst(BuiltinFunc::GetBuiltinFunc(id), args, "");
    }

    // 普通函数
    Function *func = dynamic_cast<Function *>(Singleton<Module>().GetValueByName(id));
    if (!func)
    {
        throw std::runtime_error("No such function: '" + id + "'");
    }

    // 参数
    auto &params = func->GetParams();
    if (args.size() != params.size())
    {
        throw std::invalid_argument("Function '" + id + "' expects " + std::to_string(params.size()) +
                                    " arguments, but got " + std::to_string(args.size()));
    }
    auto i = args.begin();
    for (auto j = params.begin(); j != params.end(); j++, i++)
    {
        Operand &m = *i;
        Operand n = j->get();
        if (n->GetType() != m->GetType())
            assert(0 && "wrong input type");
    }
    return new CallInst(func, args, "");
}

LoadInst::LoadInst(Type *_tp)
    : Instruction(_tp, Op::Load) {}

LoadInst::LoadInst(Operand _src)
    : Instruction(dynamic_cast<PointerType *>(_src->GetType())->GetSubType(), Op::Load)
{
    assert(GetTypeEnum() == IR_Value_INT || GetTypeEnum() == IR_Value_Float || GetTypeEnum() == IR_PTR);
    add_use(_src);
}

StoreInst::StoreInst(Type *_tp)
    : Instruction(_tp, Op::Store) {}

StoreInst::StoreInst(Operand _A, Operand _B)
{
    add_use(_A);
    add_use(_B);
    name = "StoreInst";
    id = Op::Store;
}

Operand StoreInst::GetDef()
{
    return nullptr;
}

AllocaInst::AllocaInst(Type *_tp)
    : Instruction(_tp, Op::Alloca) {}

bool AllocaInst::isUsed()
{
    auto &ValUseList = GetValUseList();
    return !ValUseList.is_empty();
}

CallInst::CallInst(Type *_tp)
    : Instruction(_tp, Op::Call) {}

CallInst::CallInst(Value *_func, std::vector<Operand> &_args, std::string label)
    : Instruction(_func->GetType(), Op::Call)
{
    name += label;
    add_use(_func);
    for (auto &n : _args)
    {
        add_use(n);
    }
}

RetInst::RetInst()
{
    id = Op::Ret;
}

RetInst::RetInst(Type *_tp)
    : Instruction(_tp, Op::Ret) {}

RetInst::RetInst(Operand op)
{
    add_use(op);
    id = Op::Ret;
}

Operand RetInst::GetDef()
{
    return nullptr;
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

BinaryInst::BinaryInst(Operand _A, Operation __op, Operand _B, bool Atom)
    : Instruction(isBinaryBool(__op) ? BoolType::NewBoolTypeGet()
                                     : _A->GetType())
{
    op = __op;
    // 与User中的OpID对应
    id = static_cast<Instruction::Op>(__op + 8);
    add_use(_A);
    add_use(_B);
    Atomic = Atom;
}

CondInst::CondInst(Type *_tp)
    : Instruction(_tp, Op::Cond) {}

CondInst::CondInst(Operand _cond, BasicBlock *_true, BasicBlock *_false)
{
    add_use(_cond);
    add_use(_true);
    add_use(_false);
    id = Op::Cond;
}

Operand CondInst::GetDef()
{
    return nullptr;
}

UnCondInst::UnCondInst(Type *_tp)
    : Instruction(_tp, Op::UnCond) {}

UnCondInst::UnCondInst(BasicBlock *_BB)
{
    add_use(_BB);
    id = Op::UnCond;
}

Operand UnCondInst::GetDef()
{
    return nullptr;
}

// bool isBinaryBool(BinaryInst::Operation _op)
// {
//   switch (_op)
//   {
//   case BinaryInst::Op_E:
//   case BinaryInst::Op_NE:
//   case BinaryInst::Op_G:
//   case BinaryInst::Op_GE:
//   case BinaryInst::Op_L:
//   case BinaryInst::Op_LE:
//     return true;
//   default:
//     return false;
//   }
// }

ZextInst::ZextInst(Type *_tp) : Instruction(_tp, Op::Zext) {}

ZextInst::ZextInst(Operand ptr) : Instruction(IntType::NewIntTypeGet(), Op::Zext)
{
    add_use(ptr);
}

SextInst::SextInst(Type *_tp) : Instruction(_tp, Op::Sext) {}

SextInst::SextInst(Operand ptr) : Instruction(Int64Type::NewInt64TypeGet(), Op::Sext)
{
    add_use(ptr);
}

TruncInst::TruncInst(Type *_tp) : Instruction(_tp, Op::Trunc) {}

TruncInst::TruncInst(Operand ptr) : Instruction(IntType::NewIntTypeGet(), Op::Trunc)
{
    add_use(ptr);
}

MaxInst::MaxInst(Type *_tp) : Instruction(_tp, Op::Max) {}

MaxInst::MaxInst(Operand _A, Operand _B) : Instruction(_A->GetType(), Op::Max)
{
    add_use(_A);
    add_use(_B);
    id = Op::Max;
}

MinInst::MinInst(Type *_tp) : Instruction(_tp, Op::Min) {}

MinInst::MinInst(Operand _A, Operand _B) : Instruction(_A->GetType(), Op::Min)
{
    add_use(_A);
    add_use(_B);
    id = Op::Min;
}

SelectInst::SelectInst(Type *_tp) : Instruction(_tp, Op::Select) {}

SelectInst::SelectInst(Operand _cond, Operand _true, Operand _false) : Instruction(_true->GetType(), Op::Select)
{
    add_use(_cond);
    add_use(_true);
    add_use(_false);
}

GepInst::GepInst(Type *_tp) : Instruction(_tp, Op::Gep) {}

GepInst::GepInst(Operand base)
{
    add_use(base);
    id = Op::Gep;
}

GepInst::GepInst(Operand base, std::vector<Operand> &args)
{
    add_use(base);
    for (auto &&i : args)
        add_use(i);
    id = Op::Gep;
}

void GepInst::AddArg(Value *arg)
{
    add_use(arg);
}

Type *GepInst::GetType()
{
    int limi = useruselist.size() - 1;
    type = useruselist[0]->GetValue()->GetType();
    for (int i = 1; i <= limi; i++)
        type = dynamic_cast<HasSubType *>(type)->GetSubType();
    return type = PointerType::NewPointerTypeGet(type);
}

std::vector<Operand> GepInst::GetIndexs()
{
    std::vector<Operand> indexs;
    for (int i = 1; i < useruselist.size(); i++)
        indexs.push_back(useruselist[i]->GetValue());
    return indexs;
}

// 类型转换
Operand ToFloat(Operand op, BasicBlock *block)
{
    if (op->GetType() == FloatType::NewFloatTypeGet())
        return op;
    if (op->isConst())
    {
        if (auto op_int = dynamic_cast<ConstIRInt *>(op))
            return ConstIRFloat::GetNewConstant((float)op_int->GetVal());
        else if (auto op_float = dynamic_cast<ConstIRBoolean *>(op))
            return ConstIRFloat::GetNewConstant((float)op_float->GetVal());
        else
            assert(false);
    }
    else
    {
        assert(block != nullptr);
        if (op->GetType() == IntType::NewIntTypeGet())
            return block->GenerateSI2FPInst(op);
        else
            assert(false);
    }
}

Operand ToInt(Operand op, BasicBlock *block)
{
    if (op->GetType() == IntType::NewIntTypeGet())
        return op;
    if (op->isConst())
    {
        if (auto op_int = dynamic_cast<ConstIRBoolean *>(op))
            return ConstIRInt::GetNewConstant((int)op_int->GetVal());
        else if (auto op_float = dynamic_cast<ConstIRFloat *>(op))
            return ConstIRInt::GetNewConstant((int)op_float->GetVal());
        else
            assert(false);
    }
    else
    {
        assert(block != nullptr);
        if (op->GetType() == FloatType::NewFloatTypeGet())
            return block->GenerateFP2SIInst(op);
        else
            assert(false);
    }
}

/// @brief  PhiInst
/// 最奖励的一节
PhiInst::PhiInst(Type *_tp) : oprandNum(0), Instruction(_tp, Op::Phi) {}

PhiInst::PhiInst(Instruction *BeforeInst, Type *_tp) : oprandNum(0), Instruction(_tp, Op::Phi) {}

PhiInst::PhiInst(Instruction *BeforeInst) : oprandNum(0)
{
    id = Op::Phi;
}

// PhiInst *PhiInst::clone(std::unordered_map<Operand, Operand> &mapping)
// {
// }

// void PhiInst::print()
// {
// }

PhiInst *PhiInst::Create(Type *type)
{
    assert(type);
    PhiInst *tmp = new PhiInst(type);
    tmp->id = Op::Phi;
    return tmp;
}

PhiInst *PhiInst::Create(Instruction *BeforeInst, BasicBlock *currentBB,
                         std::string Name)
{
    //// beforeInst 与 phiInst ？？？
    PhiInst *tmp = new PhiInst{BeforeInst};
    // for(auto I = currentBB->begin(),
    //          E = currentBB->end(); I != E ; ++I){
    //     if(*I == BeforeInst)
    //         I.InsertBefore(tmp);
    //     else
    //         assert("I create all the phi is the first Inst in BB");
    // }

    auto I = currentBB->begin();
    if (*I == BeforeInst)
        I.InsertBefore(tmp);
    else
        assert("I create all the phi is the first Inst in BB");

    if (!Name.empty())
        tmp->SetName(Name);
    tmp->id = Op::Phi;
    return tmp;
}

PhiInst *PhiInst::Create(Instruction *BeforeInst,
                         BasicBlock *currentBB, Type *type,
                         std::string Name)
{
    PhiInst *tmp = new PhiInst(BeforeInst, type);
    auto I = currentBB->begin();
    if (*I == BeforeInst)
        I.InsertBefore(tmp);
    else
        assert("I create all the phi is the first Inst in BB");

    if (!Name.empty())
        tmp->SetName(Name);
    tmp->id = Op::Phi;
    return tmp;
}

void PhiInst::addIncoming(Value *IncomingVal, BasicBlock *PreBB)
{
    // build the bond bettwen user and value
    add_use(IncomingVal);
    auto &use = useruselist.back();
    // 记录了关系
    PhiRecord[oprandNum] = std::make_pair(IncomingVal, PreBB);
    UseRecord[use.get()] = oprandNum;
    oprandNum++;
}

BasicBlock *PhiInst::getIncomingBlock(int num)
{
    auto &[v, bb] = PhiRecord[num];
    return bb;
}

Value *PhiInst::getIncomingValue(int num)
{
    auto &[v, bb] = PhiRecord[num];
    return v;
}

std::vector<Value *> &PhiInst::RecordIncomingValsA_Blocks()
{
    Incomings.clear();
    IncomingBlocks.clear();

    for (const auto &[_1, value] : PhiRecord)
    {
        Incomings.push_back(value.first);
        IncomingBlocks.push_back(value.second);
    }
    return Incomings;
}

bool PhiInst::IsReplaced()
{
    Value *tmp = Incomings[0];
    bool ret = true;
    for (auto e : Incomings)
    {
        // 他们用的都是同一个值,内存值相同
        if (tmp == e)
            continue;
        else
            ret = false;

        if (!ret)
            break;
        tmp = e;
    }

    return ret;
}

//////// end

ConstantData *ConstantData::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return this;
}

UndefValue *UndefValue::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return this;
}

Value *Var::clone(std::unordered_map<Operand, Operand> &mapping)
{
    if (this->usage == Var::Constant ||
        this->usage == Var::GlobalVar)
        return this;
    else
    {
        assert(mapping.find(this) != mapping.end() && "variable not copied!");
        return mapping[this];
    }
}

LoadInst *LoadInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<LoadInst>(this, mapping);
}

StoreInst *StoreInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<StoreInst>(this, mapping);
}

AllocaInst *AllocaInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<AllocaInst>(this, mapping);
}

CallInst *CallInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<CallInst>(this, mapping);
}

RetInst *RetInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<RetInst>(this, mapping);
}

CondInst *CondInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<CondInst>(this, mapping);
}

UnCondInst *UnCondInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<UnCondInst>(this, mapping);
}

BinaryInst *BinaryInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    auto tmp = normal_clone<BinaryInst>(this, mapping);
    tmp->op = op;
    tmp->id = static_cast<Instruction::Op>(op + 8);
    tmp->Atomic = Atomic;
    return tmp;
}

ZextInst *ZextInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<ZextInst>(this, mapping);
}

SextInst *SextInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<SextInst>(this, mapping);
}

TruncInst *TruncInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<TruncInst>(this, mapping);
}

MaxInst *MaxInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<MaxInst>(this, mapping);
}

MinInst *MinInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<MinInst>(this, mapping);
}

SelectInst *SelectInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<SelectInst>(this, mapping);
}

GepInst *GepInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<GepInst>(this, mapping);
}

FP2SIInst *FP2SIInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<FP2SIInst>(this, mapping);
}

SI2FPInst *SI2FPInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
    return normal_clone<SI2FPInst>(this, mapping);
}

// BB
// std::vector<BasicBlock::InstPtr> &BasicBlock::GetInsts()
// {
//     return instructions;
// }

BasicBlock::BasicBlock()
    : Value(VoidType::NewVoidTypeGet()), LoopDepth(0), visited(false), index(0), reachable(false)
{
}

BasicBlock::~BasicBlock() = default;

// void BasicBlock::init_Insts()
// {
//     instructions.clear();
//     NextBlocks.clear();
//     PredBlocks.clear();
// }

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
    std::cout << GetName() << ":\n";
    for (auto i : (*this))
    {
        std::cout << "  ";
        i->print();
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

// bool BasicBlock::is_empty_Insts() const
// {
//     return instructions.empty();
// }

// Instruction *BasicBlock::GetLastInsts() const
// {
//     return is_empty_Insts() ? nullptr : instructions.back().get();
// }

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

    auto storeinst = new StoreInst(src, des);
    this->push_back(storeinst);
    // 要同时开启才开
    //  instructions.emplace_back(std::move(storeinst));
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
// 再改
Operand BasicBlock::GenerateCallInst(std::string id, std::vector<Operand> args,
                                     int run_time)
{
    auto check_builtin = [](std::string _id)
    {
        if (_id == "getint")
            return true;
        if (_id == "getfloat")
            return true;
        if (_id == "getch")
            return true;
        if (_id == "getarray")
            return true;
        if (_id == "getfarray")
            return true;
        if (_id == "putint")
            return true;
        if (_id == "putch")
            return true;
        if (_id == "putarray")
            return true;
        if (_id == "putfloat")
            return true;
        if (_id == "putfarray")
            return true;
        if (_id == "starttime")
            return true;
        if (_id == "stoptime")
            return true;
        if (_id == "putf")
            return true;
        if (_id == "llvm.memcpy.p0.p0.i32")
            return true;
        return false;
    };

    if (check_builtin(id))
    {
        if (id == "starttime" || id == "stoptime")
        {
            assert(args.size() == 0);
            args.push_back(ConstIRInt::GetNewConstant(run_time));
        }

        if (id == "putint" || id == "putch" || id == "putarray" ||
            id == "putfarray")
        {
            if (args[0]->GetTypeEnum() == IR_Value_Float)
                args[0] = GenerateFP2SIInst(args[0]);
        }
        if (id == "putfloat")
        {
            if (args[0]->GetTypeEnum() == IR_Value_INT)
                args[0] = GenerateSI2FPInst(args[0]);
        }
        auto tmp = new CallInst(BuiltinFunc::GetBuiltinFunc(id), args,
                                "at" + std::to_string(run_time));
        push_back(tmp);
        return tmp->GetDef();
    }
    if (auto func =
            dynamic_cast<Function *>(Singleton<Module>().GetValueByName(id)))
    {
        auto &params = func->GetParams();
        assert(args.size() == params.size());
        auto i = args.begin();
        for (auto j = params.begin(); j != params.end(); j++, i++)
        {
            auto &ii = *i;
            auto jj = j->get();
            if (jj->GetType() != ii->GetType())
            {
                auto a = ii->GetType()->GetTypeEnum(), b = jj->GetType()->GetTypeEnum();
                assert(a == IR_Value_INT || a == IR_Value_Float);
                assert(b == IR_Value_INT || b == IR_Value_Float);
                if (b == IR_Value_Float)
                    ii = GenerateSI2FPInst(ii);
                else
                    ii = GenerateFP2SIInst(ii);
            }
        }
        auto inst = new CallInst(func, args, "at" + std::to_string(run_time));
        push_back(inst);
        return inst->GetDef();
    }
    else
    {
        std::cerr << "No Such Function!\n";
        assert(0);
    }
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

bool BasicBlock::IsEnd()
{
    if (auto data = dynamic_cast<UnCondInst *>(GetBack()))
        return 1;
    else if (auto data = dynamic_cast<CondInst *>(GetBack()))
        return 1;
    else if (auto data = dynamic_cast<RetInst *>(GetBack()))
        return 1;
    else
        return 0;
}

// Function
Function::Function(IR_DataType _type, const std::string &_id)
    : Value(NewTypeFromIRDataType(_type), _id)
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
    for (auto BB : (*this)) // 链表打印
        BB->print();
    std::cout << "}\n";
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
    insert->GenerateUnCondInst(curr);
    insert->size_Inst = this->size_BB++;
    this->PushBothBB(insert);
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
    for (const auto bb : *this)
    {
        auto inst = bb->GetBack();
        if (dynamic_cast<RetInst *>(inst))
        {
            tmp.push_back(bb);
            continue;
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

UndefValue *UndefValue::Get(Type *_ty)
{
    static std::map<Type *, UndefValue *> Undefs;
    UndefValue *&UnVal = Undefs[_ty];
    if (!UnVal)
        UnVal = new UndefValue(_ty);
    return UnVal;
}