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
    //...
  }
  // 浮点常量
  if (val->isConst() && val->GetType() == FloatType::NewFloatTypeGet())
  {
    //...
  }
  if (val2mop.find(val) == val2mop.end())
  {
    val2mop[val] = Create(val);
    ///@todo Create
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
      // 访问栈帧对象
      auto &frameobjs = cur_func->GetFrame()->GetFrameObjs();
      frameobjs.emplace_back(new RISCVFrameObject(inst));

      auto subtype = dynamic_cast<HasSubType *>(inst->GetType())->GetSubType();
      if (dynamic_cast<ArrayType *>(subtype)) // 数组类型，首地址很重要
      {
        // return cur_func->GetUsedGlobalMapping();
      }
    }
  }
  else if (auto bb = dynamic_cast<BasicBlock *>(val))
  {
  }
  else if (val->isConst())
  {
  }
  else if (auto func = dynamic_cast<Function *>(val))
  {
    return new RISCVFunction(func);
  }
}