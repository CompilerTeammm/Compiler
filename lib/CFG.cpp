#pragma once
#include "../include/lib/CoreClass.hpp"
#include "../include/lib/CFG.hpp"

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

ConstantData *ConstantData::clone(std::unordered_map<Operand, Operand> &mapping)
{
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

UndefValue *UndefValue::Get(Type *_ty)
{
    static std::map<Type *, UndefValue *> Undefs;
    UndefValue *&UnVal = Undefs[_ty];
    if (!UnVal)
        UnVal = new UndefValue(_ty);
    return UnVal;
}

Var::Var(UsageTag tag, Type *_tp, std::string _id)
    : User(tag == Param ? _tp : PointerType::NewPointerTypeGet(_tp)),
      usage(tag)
{
    if (usage == Param)
        return;
    if (usage == GlobalVar)
    {
        name = ".G." + _id;
        Singleton<Module>().Register(_id, this);
    }
    else if (usage == Constant)
        name = ".C." + name;
    Singleton<Module>().PushVar(this);
}

LoadInst::LoadInst(Type *_tp)
    : Instruction(_tp, Op::Load) {}

LoadInst::LoadInst(Operand _src)
    : Instruction(dynamic_cast<PointerType *>(_src->GetType())->GetSubType(), Op::Load)
{
    assert(GetTypeEnum() == IR_Value_INT || GetTypeEnum() == IR_Value_Float || GetTypeEnum() == IR_PTR);
    add_use(_src);
}

LoadInst *LoadInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void LoadInst::print()
{
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

StoreInst *StoreInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void StoreInst::print()
{
}

AllocaInst::AllocaInst(Type *_tp)
    : Instruction(_tp, Op::Alloca) {}

bool AllocaInst::isUsed()
{
    auto &ValUseList = GetValUseList();
    return !ValUseList.is_empty();
}

AllocaInst *AllocaInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void AllocaInst::print()
{
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

CallInst *CallInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void CallInst::print()
{
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

RetInst *RetInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void RetInst::print()
{
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

CondInst *CondInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void CondInst::print()
{
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

UnCondInst *UnCondInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void UnCondInst::print()
{
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

ZextInst *ZextInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void ZextInst::print()
{
}

SextInst::SextInst(Type *_tp) : Instruction(_tp, Op::Sext) {}

SextInst::SextInst(Operand ptr) : Instruction(Int64Type::NewInt64TypeGet(), Op::Sext)
{
    add_use(ptr);
}

SextInst *SextInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void SextInst::print()
{
}

TruncInst::TruncInst(Type *_tp) : Instruction(_tp, Op::Trunc) {}

TruncInst::TruncInst(Operand ptr) : Instruction(IntType::NewIntTypeGet(), Op::Trunc)
{
    add_use(ptr);
}

TruncInst *TruncInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void TruncInst::print()
{
}

MaxInst::MaxInst(Type *_tp) : Instruction(_tp, Op::Max) {}

MaxInst::MaxInst(Operand _A, Operand _B) : Instruction(_A->GetType(), Op::Max)
{
    add_use(_A);
    add_use(_B);
    id = Op::Max;
}

MaxInst *MaxInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void MaxInst::print()
{
}

MinInst::MinInst(Type *_tp) : Instruction(_tp, Op::Min) {}

MinInst::MinInst(Operand _A, Operand _B) : Instruction(_A->GetType(), Op::Min)
{
    add_use(_A);
    add_use(_B);
    id = Op::Min;
}

MinInst *MinInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void MinInst::print()
{
}

SelectInst::SelectInst(Type *_tp) : Instruction(_tp, Op::Select) {}

SelectInst::SelectInst(Operand _cond, Operand _true, Operand _false) : Instruction(_true->GetType(), Op::Select)
{
    add_use(_cond);
    add_use(_true);
    add_use(_false);
}

SelectInst *SelectInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void SelectInst::print()
{
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
}

std::vector<Operand> GepInst::GetIndexs()
{
    std::vector<Operand> indexs;
    for (int i = 1; i < useruselist.size(); i++)
        indexs.push_back(useruselist[i]->GetValue());
    return indexs;
}

GepInst *GepInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void GepInst::print()
{
}

// 类型转换
Operand ToFloat(Operand op, BasicBlock *bb)
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
        assert(bb != nullptr);
        if (op->GetType() == IntType::NewIntTypeGet())
            return bb->GenerateSI2FPInst(op);
        else
            assert(false);
    }
}

Operand ToInt(Operand op, BasicBlock *bb)
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
        assert(bb != nullptr);
        if (op->GetType() == FloatType::NewFloatTypeGet())
            return bb->GenerateFP2SIInst(op);
        else
            assert(false);
    }
}

PhiInst::PhiInst(Type *_tp) : oprandNum(0), Instruction(_tp, Op::Phi) {}

PhiInst::PhiInst(Instruction *BeforeInst, Type *_tp) : oprandNum(0), Instruction(_tp, Op::Phi) {}

PhiInst::PhiInst(Instruction *BeforeInst) : oprandNum(0)
{
    id = Op::Phi;
}

PhiInst *PhiInst::clone(std::unordered_map<Operand, Operand> &mapping)
{
}

void PhiInst::print()
{
}