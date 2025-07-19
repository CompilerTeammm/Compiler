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

// void Initializer::Var2Store(BasicBlock *block, const std::string &name, std::vector<int> &gep_data)
// {
//     auto module = Singleton<Module>().GetValueByName(name);
//     auto base_gep = dynamic_cast<GepInst *>(block->GenerateGepInst(module));

//     for (size_t i = 0; i < this->size(); i++)
//     {
//         auto &handle = (*this)[i];
//         gep_data.push_back(i);
//         if (auto inits = dynamic_cast<Initializer *>(handle))
//         {
//             inits->Var2Store(block, name, gep_data);
//         }
//         else
//         {
//             if (!handle->isConst())
//             {
//                 auto gep = base_gep;
//                 for (int j : gep_data)
//                     gep->add_use(ConstIRInt::GetNewConstant(j));

//                 block->GenerateStoreInst(handle, gep);
//                 if (handle->GetType()->GetTypeEnum() == IR_Value_INT)
//                     handle = ConstIRInt::GetNewConstant();
//                 else
//                     handle = ConstIRFloat::GetNewConstant();
//             }
//         }
//         gep_data.pop_back();
//     }
// }
// nme
void Initializer::Var2Store(BasicBlock *bb, const std::string &name,
                            std::vector<int> &gep_data)
{
    for (int i = 0; i < this->size(); i++)
    {
        auto &handle = (*this)[i];
        gep_data.push_back(i);
        if (auto inits = dynamic_cast<Initializer *>(handle))
        {
            inits->Var2Store(bb, name, gep_data);
        }
        else
        {
            if (!handle->isConst())
            {
                auto gep = dynamic_cast<GepInst *>(
                    bb->GenerateGepInst(Singleton<Module>().GetValueByName(name)));
                gep->add_use(ConstIRInt::GetNewConstant());
                for (auto &j : gep_data)
                    gep->add_use(ConstIRInt::GetNewConstant(j));
                bb->GenerateStoreInst(handle, gep);
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

// const std::string &_id//me
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
        {"CacheLookUp64", VoidType::NewVoidTypeGet()},

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

// BuiltinFunc *BuiltinFunc::GetBuiltinFunc(std::string _id)
// {
//     static std::map<std::string, BuiltinFunc *> mp;
//     auto get_type = [&_id]() -> Type *
//     {
//         if (_id == "getint")
//             return IntType::NewIntTypeGet();
//         if (_id == "getfloat")
//             return FloatType::NewFloatTypeGet();
//         if (_id == "getch")
//             return IntType::NewIntTypeGet();
//         if (_id == "getarray")
//             return IntType::NewIntTypeGet();
//         if (_id == "getfarray")
//             return IntType::NewIntTypeGet();
//         if (_id == "putint")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "putch")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "putarray")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "putfloat")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "putfarray")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "starttime")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "stoptime")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "putf")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "llvm.memcpy.p0.p0.i32")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "memcpy@plt")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "buildin_NotifyWorkerLE")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "buildin_NotifyWorkerLT")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "buildin_FenceArgLoaded")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "buildin_AtomicF32add")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "CacheLookUp")
//             return VoidType::NewVoidTypeGet();
//         if (_id == "CacheLookUp4")
//             return VoidType::NewVoidTypeGet();
//         assert(0);
//     };
//     if (mp.find(_id) == mp.end())
//     {
//         mp[_id] = new BuiltinFunc(get_type(), _id);
//     }
//     return mp[_id];
// }

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
    : Instruction(_tp)
{
    id = Op::Alloca;
}

bool AllocaInst::isUsed()
{
    auto &ValUseList = GetValUseList();
    return !ValUseList.is_empty();
}

CallInst::CallInst(Type *_tp)
    : Instruction(_tp, Op::Call) {}

CallInst::CallInst(Value *_func, std::vector<Operand> &_args, std::string label)
    : Instruction(_func->GetType(), Op::Call),CalledFunction(_func)
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

void PhiInst::print()
{
    int count = 0;
    dynamic_cast<Value *>(this)->print();
    std::cout << " = phi ";
    this->GetType()->print();
    std::cout << " ";
    for (auto &[_1, value] : PhiRecord)
    {
        std::cout << "[";
        value.first->print();
        std::cout << ", ";
        dynamic_cast<Value *>(value.second)->print();
        std::cout << "]";
        if (count != PhiRecord.size() - 1)
            std::cout << ", ";
        count++;
    }
    std::cout << "\n";
    return;
}

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

Value *PhiInst::ReturnValIn(BasicBlock *bb)
{
    auto it = std::find_if(
        PhiRecord.begin(), PhiRecord.end(),
        [bb](const std::pair<const int, std::pair<Value *, BasicBlock *>> &ele)
        {
            return ele.second.second == bb;
        });
    if (it == PhiRecord.end())
        return nullptr;
    return it->second.first;
}

void PhiInst::Del_Incomes(int CurrentNum)
{
    if (PhiRecord.find(CurrentNum) != PhiRecord.end())
    {
        auto iter = std::find_if(
            uselist.begin(), uselist.end(), [=](const std::unique_ptr<Use> &ele)
            { return ele->GetValue() == PhiRecord[CurrentNum].first; });
        auto &vec = GetUserUseList();
        // (*iter)->RemoveFromUserList((*iter)->GetUser());
        for (int i = 0; i < vec.size(); i++)
            if (vec[i].get() == (*iter).get())
                vec.erase(std::remove(vec.begin(), vec.end(), (*iter)));
        PhiRecord.erase(CurrentNum);
        auto i = std::find_if(
            UseRecord.begin(), UseRecord.end(),
            [CurrentNum](auto &ele)
            { return ele.second == CurrentNum; });
        if (i != UseRecord.end())
        {
            UseRecord.erase(i);
        }
    }
    else
        std::cerr << "No such PhiRecord" << std::endl;
}

void PhiInst::FormatPhi()
{
    std::vector<std::pair<int, std::pair<Value *, BasicBlock *>>> assist;
    std::queue<std::pair<Value *, BasicBlock *>> defend;
    UseRecord.clear();
    oprandNum = 0;
    for (auto &[_1, v] : PhiRecord)
    {
        assist.push_back(std::make_pair(_1, v));
        // defend.push(std::make_pair(v.first, v.second));
    }
    std::sort(assist.begin(), assist.end(),
              [](const auto &p1, const auto &p2)
              { return p1.first < p2.first; });
    PhiRecord.clear();
    for (int i = 0; i < assist.size(); i++)
    {
        defend.push(
            std::make_pair(assist[i].second.first, assist[i].second.second));
    }
    while (!defend.empty())
    {
        auto &[v_fir, v_sec] = defend.front();
        defend.pop();
        PhiRecord[oprandNum++] = std::make_pair(v_fir, v_sec);
    }
    int tmp = 0;
    for (auto &use : uselist)
    {
        UseRecord[use.get()] = tmp++;
    }
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

    // C++ 17 的语法， _1 忽略键，value对应的pair<Value*.BasicBlock*>
    for (const auto &[_1, value] : PhiRecord)
    {
        Incomings.push_back(value.first);
        IncomingBlocks.push_back(value.second);
    }
    return Incomings;
}

void PhiInst::PhiProp(Value *old, Value *val)
{
    // 标记是第几个 phi的分支
    int index;
    std::vector<Value *> &comingVals = RecordIncomingValsA_Blocks();
    auto it = std::find(comingVals.begin(), comingVals.end(), old);
    if (it != comingVals.end())
    {
        index = std::distance(comingVals.begin(), it);
    }
    BasicBlock *bb = getIncomingBlock(index);
    // 更改该位置下的值
    PhiRecord[index] = std::make_pair(val, bb);
}

bool PhiInst::IsReplaced()
{
    RecordIncomingValsA_Blocks();
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

void BasicBlock::RemoveNextBlock(BasicBlock *next)
{
    NextBlocks.erase(
        std::remove(NextBlocks.begin(), NextBlocks.end(), next),
        NextBlocks.end());
}

// void BasicBlock::RemovePredBB(BasicBlock *pre)
// {
//     if (pre == this)
//     {
//         for (auto iter = pre->begin();
//              iter != pre->end() && dynamic_cast<PhiInst *>(*iter);)
//         {
//             auto phi = dynamic_cast<PhiInst *>(*iter);
//             ++iter;
//             phi->EraseRecordByBlock(pre);
//             if (phi->PhiRecord.size() == 1)
//             {
//                 BasicBlock *b = phi->PhiRecord.begin()->second.second;
//                 if (b == this)
//                 {
//                     phi->ReplaceAllUseWith(UndefValue::Get(phi->GetType()));
//                     delete phi;
//                 }
//                 else
//                 {
//                     Value *repl = (*(phi->PhiRecord.begin())).second.first;
//                     if (repl == phi)
//                         phi->ReplaceAllUseWith(UndefValue::Get(phi->GetType()));
//                     phi->ReplaceAllUseWith(repl);
//                     delete phi;
//                 }
//             }
//         }
//         return;
//     }
//     for (auto iter = this->begin(); iter != this->end(); ++iter)
//     {
//         auto inst = *iter;
//         if (auto phi = dynamic_cast<PhiInst *>(this->front()))
//         {
//             auto tmp = std::find_if(
//                 phi->PhiRecord.begin(), phi->PhiRecord.end(),
//                 [pre](const std::pair<int, std::pair<Value *, BasicBlock *>> &ele)
//                 {
//                     return ele.second.second == pre;
//                 });
//             if (tmp != phi->PhiRecord.end())
//             {
//                 phi->Del_Incomes(tmp->first);
//                 phi->FormatPhi();
//             }
//             if (phi->PhiRecord.size() == 1)
//             {
//                 BasicBlock *b = phi->PhiRecord.begin()->second.second;
//                 if (b == this)
//                 {
//                     phi->ReplaceAllUseWith(UndefValue::Get(phi->GetType()));
//                     delete phi;
//                 }
//                 else
//                 {
//                     Value *repl = (*(phi->PhiRecord.begin())).second.first;
//                     if (repl == phi)
//                         phi->ReplaceAllUseWith(UndefValue::Get(phi->GetType()));
//                     phi->ReplaceAllUseWith(repl);
//                     delete phi;
//                 }
//             }
//         }
//         else
//             return;
//     }
// }

// bool BasicBlock::is_empty_Insts() const
// {
//     return instructions.empty();
// }

// Instruction *BasicBlock::GetLastInsts() const
// {
//     return is_empty_Insts() ? nullptr : instructions.back().get();
// }

// dh
Instruction *BasicBlock::GetLastInsts() const
{
    return this->GetBack();
}

Instruction* BasicBlock::GetFirstInsts() const {
    return this->GetFront();  
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
    if (des->GetType()->GetTypeEnum() != IR_PTR)
    {
        std::cerr << "Error: des is not IR_PTR, actual type: "
                  << des->GetType()->GetTypeEnum() << std::endl;
    }
    assert(des->GetType()->GetTypeEnum() == IR_PTR);
    auto tmp = dynamic_cast<PointerType *>(des->GetType());

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
    if (BuiltinFunc::CheckBuiltin(id))
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
}

void Function::InsertBB(BasicBlock *pred, BasicBlock *succ, BasicBlock *insert)
{
}

void Function::InsertBB(BasicBlock *curr, BasicBlock *insert)
{
    insert->GenerateUnCondInst(curr);
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
    }
}

void Function::InitBBs()
{
    BBs.clear();
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

bool Module::EraseDeadFunc() {
  std::vector<Function *> erase;
  bool changed = false;
  for (auto &func : functions) {
    if (func->GetValUseListSize() == 0 && func->GetName() != "main") {
      erase.push_back(func.get());
      changed = true;
    }
  }
  if (!erase.empty())
    for (auto func : erase) {
      EraseFunction(func);
    }
  return changed;
}

UndefValue *UndefValue::Get(Type *_ty)
{
    static std::map<Type *, UndefValue *> Undefs;
    UndefValue *&UnVal = Undefs[_ty];
    if (!UnVal)
        UnVal = new UndefValue(_ty);
    return UnVal;
}

void PhiInst::removeIncomingFrom(BasicBlock *fromBB)
{
    // 要移除的索引集合
    std::vector<int> toEraseIndices;

    // 找出所有与BB匹配的索引
    for (const auto &entry : PhiRecord)
    {
        int idx = entry.first;
        BasicBlock *curBB = entry.second.second;
        if (curBB == fromBB)
        {
            toEraseIndices.push_back(idx);
        }
    }
    // 反向排序，确保删除顺序不会影响vector索引
    std::sort(toEraseIndices.rbegin(), toEraseIndices.rend());

    for (int idx : toEraseIndices)
    {
        // 从UseRecord中移除相应的Use*
        auto val = PhiRecord[idx].first;

        // 找到UseRecord中对应的Use*
        for (auto it = UseRecord.begin(); it != UseRecord.end();)
        {
            if (it->second == idx)
            {
                Use *usePtr = it->first;
                // 从useruselist中移除对应的shared_ptr
                auto uit = std::find_if(useruselist.begin(), useruselist.end(),
                                        [usePtr](const std::unique_ptr<Use> &p)
                                        { return p.get() == usePtr; });
                if (uit != useruselist.end())
                {
                    useruselist.erase(uit);
                }
                it = UseRecord.erase(it);
            }
            else
            {
                ++it;
            }
        }
        // 从PhiRecord中移除对应的记录
        PhiRecord.erase(idx);
        oprandNum--;
    }
    // 如果删完了，oprandNum归零
    if (PhiRecord.empty())
    {
        oprandNum = 0;
    }
    // 如果PhiRecord为空，可能需要删除PhiInst(自己后续删吧，不在这里写了)
}
BasicBlock *BasicBlock::SplitAt(User *inst)
{
    auto tmp = new BasicBlock();

    std::vector<BasicBlock *> succ;
    if (auto cond = GetBack()->as<CondInst>())
    {
        for (int i = 1; i < 3; i++)
            succ.push_back(cond->GetOperand(i)->as<BasicBlock>());
    }
    else if (auto uncond = GetBack()->as<UnCondInst>())
    {
        succ.push_back(uncond->GetOperand(0)->as<BasicBlock>());
    }
    for (auto bb : succ)
    {
        for (auto inst : *bb)
        {
            if (auto phi = inst->as<PhiInst>())
            {
                for (auto &[ind, rec] : phi->PhiRecord)
                {
                    if (rec.second == this)
                        rec.second = tmp;
                }
            }
            else
                break;
        }
    }

    auto [left, right] = SplitList((Instruction *)inst, GetBack());
    tmp->CollectList(left, right);
    return tmp;
}

size_t Function::GetInstructionCount() const {
    size_t count = 0;
    for (auto bb = GetFront(); bb != nullptr; bb = bb->GetNextNode()) {
      count += bb->Size();
    }
    return count;
}

std::pair<size_t, size_t> &Function::GetInlineInfo() {
  if (inlineinfo.first == 0) {
    for (auto bb : *this) {
      for (auto inst : *bb) {
        inlineinfo.first++;
        if (auto alloca = dynamic_cast<AllocaInst *>(inst))
          inlineinfo.second += dynamic_cast<PointerType *>(alloca->GetType())
                                   ->GetSubType()
                                   ->GetSize();
      }
    }
  }
  return inlineinfo;
}

void BasicBlock::ForEachInstrInPredBlocks(std::function<void(Instruction *)> visitor) {
  for (auto *pred : PredBlocks) {
    for (auto *instr = pred->GetFront(); instr != nullptr; instr = instr->GetNextNode()) {
      visitor(instr);
    }
  }
}
  void BasicBlock::ForEachInstrInNextBlocks(std::function<void(Instruction *)> visitor) {
    for (auto *succ : NextBlocks) {
      for (auto *instr = succ->GetFront(); instr != nullptr; instr = instr->GetNextNode()) {
        visitor(instr);
      }
    }
  }

void Function::InsertBlockAfter(BasicBlock* pos, BasicBlock* new_bb) {
    for (auto it = this->begin(); it != this->end(); ++it) {
        if (*it == pos) {
            it.InsertAfter(new_bb);
            return;
        }
    }
    assert(false && "InsertBlockAfter: pos block not found in function");
}


std::pair<Value *, BasicBlock *> Function::InlineCall(CallInst *inst, std::unordered_map<Operand, Operand> &OperandMapping)
{
    std::pair<Value *, BasicBlock *> tmp{nullptr, nullptr};
    BasicBlock *block = inst->Node::GetParent();
    User *Jump = block->GetBack();
    Function *func = block->GetParent();
    Function *inlined_func = inst->GetOperand(0)->as<Function>();
    BasicBlock *SplitBlock = block->SplitAt(inst);
    tmp.second = SplitBlock;
    BasicBlock::List<Function, BasicBlock>::iterator Block_Pos(block);
    Block_Pos.InsertAfter(SplitBlock);
    ++Block_Pos;
    std::vector<BasicBlock *> blocks;
    int num = 1;
    for (auto &param : inlined_func->GetParams())
    {
        Value *Param = param.get();
        if (OperandMapping.find(inst->GetUserUseList()[num]->usee) !=
            OperandMapping.end())
            OperandMapping[Param] = OperandMapping[inst->GetUserUseList()[num]->usee];
        else
            OperandMapping[Param] = inst->GetUserUseList()[num]->usee;
        num++;
    }
    for (BasicBlock *block : *inlined_func)
        blocks.push_back(block->clone(OperandMapping));
    UnCondInst *Br = new UnCondInst(blocks[0]);
    block->push_back(Br);
    for (auto it = blocks[0]->begin(); it != blocks[0]->end();)
    {
        auto shouldmvinst = dynamic_cast<AllocaInst *>(*it);
        ++it;
        if (shouldmvinst)
        {
            BasicBlock *front_block = func->GetFront();
            shouldmvinst->Node::EraseFromManager();
            front_block->push_front(shouldmvinst);
        }
    }
    for (BasicBlock *block_ : blocks)
        Block_Pos.InsertBefore(block_);
    if (inlined_func->GetValUseListSize() == 0)
        Singleton<Module>().EraseFunction(inlined_func);
    Value *Ret_Val = nullptr;
    if (inst->GetTypeEnum() != IR_DataType::IR_Value_VOID)
    {
        for (BasicBlock *block : blocks)
        {
            User *inst = block->GetBack();
            if (dynamic_cast<RetInst *>(inst))
            {
                if (auto val = inst->GetOperand(0))
                {
                    tmp.first = val;
                }
                UnCondInst *Br = new UnCondInst(SplitBlock);
                inst->clear_use();
                auto it = dynamic_cast<UnCondInst *>(inst);
                it->EraseFromManager();
                block->push_back(Br);
            }
        }
    }
    else
    {
        for (BasicBlock *block : blocks)
        {
            User *inst = block->GetBack();
            if (dynamic_cast<RetInst *>(inst))
            {
                UnCondInst *Br = new UnCondInst(SplitBlock);
                inst->clear_use();
                auto it = dynamic_cast<UnCondInst *>(inst);
                it->EraseFromManager();
                block->push_back(Br);
            }
        }
    }
    if (dynamic_cast<UnCondInst *>(Jump))
    {
        BasicBlock *bb = Jump->GetOperand(0)->as<BasicBlock>();
        auto iter = bb->begin();
        while (auto phi = dynamic_cast<PhiInst *>(*iter))
        {
            phi->ModifyBlock(block, SplitBlock);
            ++iter;
        }
    }
    else if (dynamic_cast<CondInst *>(Jump))
    {
        BasicBlock *bb = Jump->GetOperand(1)->as<BasicBlock>();
        auto iter = bb->begin();
        while (auto phi = dynamic_cast<PhiInst *>(*iter))
        {
            phi->ModifyBlock(block, SplitBlock);
            ++iter;
        }
        BasicBlock *bb1 = Jump->GetOperand(2)->as<BasicBlock>();
        auto iter1 = bb1->begin();
        while (auto phi = dynamic_cast<PhiInst *>(*iter1))
        {
            phi->ModifyBlock(block, SplitBlock);
            ++iter1;
        }
    }
    return tmp;
}

void PhiInst::ModifyBlock(BasicBlock *Old, BasicBlock *New)
{
    auto it1 = std::find_if(
        PhiRecord.begin(), PhiRecord.end(),
        [Old](const std::pair<int, std::pair<Value *, BasicBlock *>> &ele)
        {
            return ele.second.second == Old;
        });
    if (it1 == PhiRecord.end())
        return;
    // 将value对应的块信息更改
    it1->second.second = New;
}

void PhiInst::ReplaceVal(Use *use, Value *new_val)
{
    assert(UseRecord.find(use) != UseRecord.end());
    auto index = UseRecord[use];
    RSUW(use, new_val);
    PhiRecord[index].first = new_val;
}