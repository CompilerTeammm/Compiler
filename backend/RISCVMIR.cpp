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
  // 处理立即数
  if (auto imm = val->as<Imm>())
  {
    if (auto immi32 = imm->GetData()->as<ConstIRInt>())
    {
      if (immi32->GetVal() == 0)
      {
        return PhyRegister::GetPhyReg(PhyRegister::zero);
      }
    }
  }
  // 如果不是立即数类型，那就查找全局变量
  // usedGlobals是操作码和虚拟寄存器的映射
  if (usedGlobals.find(val) == usedGlobals.end()) // 查找 moperand和虚拟寄存器 的键值对。
  {
    auto mir = CreateSpecialUsageMIR(val); // 直接创建MIR
    usedGlobals[val] = mir->GetDef()->as<VirRegister>;
    auto entryblock = GetFront();
    entryblock->push_front(mir);
  }
  return usedGlobals[val];
}

RISCVMOperand *&RISCVMIR::GetDef() { return def; }

RISCVFunction::RISCVFunction(Value *_func)
{
}

static RISCVBasicBlock *CreateRISCVBasicBlock()
{
}