#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"

void RISCVLoweringContext::insert_val2mop(Value *val, RISCVMOperand *mop)
{
  val2mop.insert(std::make_pair(val, mop)); // 建立一个新的value和机器操作数
}

void RISCVLoweringContext::operator()(RISCVFunction *mfunc)
{
  functions.emplace_back(mfunc); // 容器尾部创建新对象
  cur_func = mfunc;
}

RISCVFunction *&RISCVLoweringContext::GetCurFunction()
{
  return cur_func;
}

RISCVMOperand *RISCVLoweringContext::mapping(Value *val)
{
  // 全局变量
  if (val->isGlobal())
  {
    // assert(cur_func != nullptr && val2mop.find(val) != val2mop.end(val));
    return cur_func->GetUsedGlobalMapping(val2mop[val]);
  }
  // 浮点常量
  if (val->isConst() && val->GetType() == FloatType::NewFloatTypeGet())
  {
    // assert(cur_func != nullptr && val->as<ConstantData> != nullptr);
    auto imm = Imm::GetImm(val->as<ConstantData>());
    return cur_func->GetUsedGlobalMapping(imm);
  }

  if (val2mop.find(val) == val2mop.end())
  {
    val2mop[val] = Create(val);
  }
  return val2mop[val];
}

// 在容器中根据一个val，创建一个操作码映射
RISCVMOperand *RISCVLoweringContext::Create(Value *val)
{
  if (auto inst = dynamic_cast<User *>(val))
  {
    if (auto alloca = dynamic_cast<AllocaInst *>(inst))
    {
      // 访问栈帧对象(函数栈帧考虑？)
      auto &frameobjs = cur_func->GetFrame()->GetFrameObjs();
      frameobjs.emplace_back(new RISCVFrameObject(inst));

      auto subtype = dynamic_cast<HasSubType *>(inst->GetType())->GetSubType();
      if (dynamic_cast<ArrayType *>(subtype)) // 数组类型，首地址很重要
      {
        return cur_func->GetUsedGlobalMapping(frameobjs.back().get());
      }
    }
    else if (auto store = dynamic_cast<StoreInst *>(inst))
    {
      assert(0 && "Can't be Used");
    }
    else if (auto cond = dynamic_cast<CondInst *>(inst))
    {
      assert(0 && "Can't be Used");
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(inst))
    {
      assert(0 && "Can't be Used");
    }
    else if (auto ret = dynamic_cast<RetInst *>(inst))
    {
      assert(0 && "Can't be Used");
    }
    else
    {
      return createVReg(RISCVTyper(inst->GetType()));
    }
  }
  else if (auto bb = dynamic_cast<BasicBlock *>(val))
  {
    static int count = 0;
    return new RISCVBasicBlock(".LBB" + std::to_string(count++));
    // 新的BB块的命名
  }
  else if (val->isConst())
  {
    if (auto boolval = val->as<ConstIRBoolean>())
    {
      auto imm = boolval->GetVal();
      return Imm::GetImm(ConstIRInt::GetNewConstant(imm));
    }
    return Imm::GetImm(val->as<ConstantData>());
  }
  else if (auto func = dynamic_cast<Function *>(val))
  {
    return new RISCVFunction(func);
  }
  else if (auto buildin = dynamic_cast<BuiltinFunc *>(val))
  {
    return new RISCVFunction(buildin);
  }
  assert(0 && "Can't be Used");
}

VirRegister *RISCVLoweringContext::createVReg(RISCVType type)
{
  return new VirRegister(type);
}