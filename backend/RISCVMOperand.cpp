#include "../include/Backend/RISCVMOperand.hpp"

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