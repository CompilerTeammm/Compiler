#include "../include/Backend/RISCVMIR.hpp"

RISCVMOperand *&RISCVMIR::GetDef()
{
  return def;
}

RISCVMOperand *&RISCVMIR::GetOperand(int ind)
{
  assert((0 <= ind && ind < operands.size()) && "Range Assertion");
  return operands[ind];
}

const int RISCVMIR::GetOperandSize()
{
  return operands.size();
}

void RISCVMIR::SetOperand(int ind, RISCVMOperand *op)
{
  assert(0 <= ind && ind < operands.size() && "Range Assertion");
  operands[ind] = op;
}

void RISCVMIR::AddOperand(RISCVMOperand *op)
{
  operands.push_back(op);
}

void RISCVMIR::SetMopcode(RISCVISA isa)
{
  this->opcode = isa;
}

// RISCV 标准化输出
void RISCVMIR::printfull()
{
  std::string name(magic_enum::enum_name(opcode));
  // 处理指令名称
  if (name == "MarkDead")
  {
    name = "#" + name;
  }
  else
  {
    if (!name.empty() && name[0] == '_')
    {
      name.erase(0, 1);
    }
    std::replace(name.begin(), name.end(), '_', '.');
  }

  if (name == "ret")
  {
    // 基本块——>函数——>退出点
    this->GetParent()->GetParent()->GetExit()->printfull();
    std::cout << "\t" << name << " \n";
    return;
  }

  std::cout << "\t" << name << " ";
  if (name == "call")
  {
    operands[0]->print();
    std::cout << "\n";
  }
  else if (def != nullptr)
  {
    def->print();
    if (!operands.empty())
    {
      std::cout << ", ";
    }
  }

  for (int i = 0; i < operands.size(); i++)
  {
    if (i == operands.size() - 1 && opcode == RISCVMIR::_amoadd_w_aqrl)
      std::cout << "(";
    operands[i]->print();
    if (i == operands.size() - 1 && opcode == RISCVMIR::_amoadd_w_aqrl)
      std::cout << ")";
    if (i != operands.size() - 1)
      std::cout << ", ";
  }
  if (name == "fcvt.w.s")
  {
    std::cout << ", rtz";
  }
  std::cout << '\n';
}

void Terminator::RotateCondition()
{
  assert(RISCVMIR::BeginBranch < branchinst->GetOpcode() && branchinst->GetOpcode() < RISCVMIR::EndBranch);
  assert(branchinst->GetOpcode() != RISCVMIR::_j);

  // 跳转条件取反
  branchinst->SetMopcode(static_cast<RISCVMIR::RISCVISA>(branchinst->GetOpcode() ^ 1));
  branchinst->SetOperand(2, falseblock);

  // 获取分支指令的下一条指令
  auto it = List<RISCVBasicBlock, RISCVMIR>::iterator(branchinst);
  ++it;

  assert(it != branchinst->GetParent()->end()); // 防止跳空

  auto nxt_inst = *it;                           // 获取下一条指令
  assert(nxt_inst->GetOpcode() == RISCVMIR::_j); // 确保是无条件跳转指令

  nxt_inst->SetOperand(0, trueblock); // 将j指令的目标改为原来的trueblock
  std::swap(trueblock, falseblock);   // 交换true/false块的记录
}

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