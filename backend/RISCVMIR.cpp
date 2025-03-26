#include "../include/Backend/RISCVMIR.hpp"

std::unique_ptr<RISCVFrame> &RISCVFunction::GetFrame()
{
  return frame;
}

std::vector<std::unique_ptr<RISCVFrameObject>> &RISCVFrame::GetFrameObjs()
{
  return frameobjs;
}

// Processing immediate values and global variables
Register *RISCVFunction::GetUsedGlobalMapping(RISCVMOperand *val)
{
  // Immediate value processing
  if (auto imm = val->as<Imm>())
  {
    if (auto immi32 = imm->GetData()->as<ConstIRInt>())
    {
      if (immi32->GetVal() == 0)
      {
        return PhyRegister::GetPhyReg(PhyRegister::zero); // Assign virtual registers
      }
    }
  }

  // Find global variables
  if (usedGlobals.find(val) == usedGlobals.end())
  {
    auto mir = CreateSpecialUsageMIR(val);
    usedGlobals[val] = mir->GetDef()->as<VirRegister>; // Allocates the virtual register map table
    auto entryblock = GetFront();                      // Insert the register into the mapping base block
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