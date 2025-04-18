#include "../include/Backend/RISCVMOperand.hpp"
#include "../include/Backend/RISCVFrameContext.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVRegister.hpp"
#include <map>

Imm::Imm(ConstantData *_data) : RISCVMOperand(RISCVTyper(_data->GetType())), data(_data)
{
}
ConstantData *Imm::GetData()
{
  return data;
}

Imm *Imm::GetImm(ConstantData *_data)
{
  using Manager = std::unique_ptr<Imm>;
  std::map<ConstantData *, Manager> mapping;
  if (mapping.find(_data) == mapping.end())
  {
    mapping[_data] = std::make_unique<Imm>(_data);
  }
  return mapping[_data].get();
}

void Imm::print()
{
  data->print();
}

Register *RISCVMOperand::ignoreLA()
{
  if (dynamic_cast<Imm *>(this))
    return nullptr;
  else if (PhyRegister *preg = dynamic_cast<PhyRegister *>(this))
  {
    PhyRegister::PhyReg regenum = preg->Getregenum();
    using PhyReg = PhyRegister::PhyReg;
    if (regenum == PhyReg::zero || regenum == PhyReg::ra ||
        regenum == PhyReg::sp || regenum == PhyReg::gp ||
        regenum == PhyReg::tp || regenum == PhyReg::s0 ||
        regenum == PhyReg::_NULL)
      return nullptr;
    else
      return preg;
  }
  else if (LARegister *lareg = dynamic_cast<LARegister *>(this))
  {
    return lareg->GetVreg();
  }
  else if (StackRegister *sreg = dynamic_cast<StackRegister *>(this))
  {
    return sreg->GetVreg();
  }
  else if (dynamic_cast<RISCVFrameObject *>(this))
    return nullptr;
  else if (dynamic_cast<RISCVBasicBlock *>(this))
    return nullptr;
  else if (dynamic_cast<RISCVMIR *>(this))
    return nullptr;
  else if (dynamic_cast<RISCVGlobalObject *>(this))
    return nullptr;
  else if (auto reg = dynamic_cast<Register *>(this))
    return reg;
  else
    return nullptr;
}