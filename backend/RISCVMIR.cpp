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
  // ����������
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
  // ����������������ͣ��ǾͲ���ȫ�ֱ���
  // usedGlobals�ǲ����������Ĵ�����ӳ��
  if (usedGlobals.find(val) == usedGlobals.end()) // ���� moperand������Ĵ��� �ļ�ֵ�ԡ�
  {
    auto mir = CreateSpecialUsageMIR(val); // ֱ�Ӵ���MIR
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