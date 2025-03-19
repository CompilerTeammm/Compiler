#include "../include/Backend/RISCVMIR.hpp"

std::unique_ptr<RISCVFrame> &RISCVFunction::GetFrame()
{
  return frame;
}

std::vector<std::unique_ptr<RISCVFrameObject>> &RISCVFrame::GetFrameObjs()
{
  return frameobjs;
}

// 处理 立即数 和 全局变量
Register *RISCVFunction::GetUsedGlobalMapping(RISCVMOperand *val)
{
  if (auto imm = val->as<Imm>()) // val->Imm
  {
    if (auto immi32 = imm->GetData()->as<ConstIRInt>())
    {
    }
  }
}

RISCVFunction::RISCVFunction(Value *_func)
{
}