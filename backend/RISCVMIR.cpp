#include "../include/Backend/RISCVMIR.hpp"

std::unique_ptr<RISCVFrame> &RISCVFunction::GetFrame()
{
  return frame;
}

std::vector<std::unique_ptr<RISCVFrameObject>> &RISCVFrame::GetFrameObjs()
{
  return frameobjs;
}

// ���� ������ �� ȫ�ֱ���
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