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

void Terminator::makeFallthrough(RISCVBasicBlock *cand)
{
  if (trueblock == cand || falseblock == cand)
  {
    if (!isUncond())
      if (trueblock == cand)
        RotateCondition();
    implictly = true;
    auto j = branchinst->GetParent()->GetBack();
    delete j;
  }
}

RISCVBasicBlock::RISCVBasicBlock(std::string _name) : NamedMOperand(_name, RISCVType::riscv_none) {}

RISCVBasicBlock *RISCVBasicBlock::CreateRISCVBasicBlock()
{
  static int t = 0;
  return new RISCVBasicBlock(".LBB" + std::to_string(t++));
}

void RISCVBasicBlock::replace_succ(RISCVBasicBlock *Now, RISCVBasicBlock *New)
{
  for (auto it = rbegin(); it != rend(); --it)
  {
    RISCVMIR *Instruction = *it;
    RISCVMIR::RISCVISA opcode = Instruction->GetOpcode();
    if (opcode < RISCVMIR::EndBranch && opcode > RISCVMIR::BeginBranch)
    {
      bool flag = false;
      for (int i = 0; i < Instruction->GetOperandSize(); i++)
      {
        if (Instruction->GetOperand(i) == Now)
        {
          Instruction->SetOperand(i, New);
          flag = true;
        }
      }
      if (flag)
        return;
    }
  }
  assert(0 && "IMPOSSIBLE");
}

Terminator &RISCVBasicBlock::getTerminator()
{
  if (term.implictly == true) // 隐式操作
    return term;

  auto inst_size = Size();
  assert(inst_size);

  // 反向遍历操作码
  for (auto it = rbegin(); it != rend(); --it)
  {
    auto minst = *it;
    // ret
    if (minst->GetOpcode() == RISCVMIR::ret)
    {
      term.branchinst = nullptr;
      break;
    }
    // 分支
    if (RISCVMIR::BeginBranch < minst->GetOpcode() && minst->GetOpcode() < RISCVMIR::EndBranch)
    {
      // 无条件跳转
      if (minst->GetOpcode() == RISCVMIR::_j)
      {
        term.branchinst = minst;
        term.trueblock = minst->GetOperand(0)->as<RISCVBasicBlock>();
      }
      // 有条件跳转
      else
      {
        term.branchinst = minst;
        term.falseblock = minst->GetOperand(2)->as<RISCVBasicBlock>();
        std::swap(term.trueblock, term.falseblock);
        break;
      }
    }
  }
  if (term.isUncond())
    term.SetProb(1);
  return term;
}

void RISCVBasicBlock::push_before_branch(RISCVMIR *m)
{
  assert(this->Size());
  for (auto it = this->rbegin(); it != this->rend(); --it)
  {
    RISCVMIR *inst = *it;
    RISCVMIR::RISCVISA opcode = inst->GetOpcode();
    if (opcode < RISCVMIR::BeginBranch || opcode > RISCVMIR::EndBranch)
    {
      assert(!(opcode == RISCVMIR::ret));
      it.InsertAfter(m);
      return;
    }
  }
  this->push_front(m);
}

void RISCVBasicBlock::printfull()
{
  // 打印基本块标签（排除特殊退出块）
  if (GetName() != ".LBBexit")
  {
    NamedMOperand::print();
    std::cout << ":\n"; // 标签后缀换行
  }

  // 打印所有有效指令
  for (auto *minst : *this)
  {
    if (minst)
    { // 防御nullptr
      minst->printfull();
    }
  }
}

RISCVFunction::RISCVFunction(Value *_func) : RISCVGlobalObject(_func->GetType(), _func->GetName()), func(_func), exit(".LBBexit")
{
  frame.reset(new RISCVFrame(this)); // 创建栈帧
  GenerateParamNeedSpill();
  exit.GetParent();
}

uint64_t RISCVFunction::GetUsedPhyRegMask()
{
  // 通过64位掩码来为每一个物理寄存器进行标号
  uint64_t flag = 0u;
  for (auto bb : *this)
  {
    for (auto inst : *bb)
    {
      // 处理指令定义的寄存器
      if (auto contReg = inst->GetDef())
      {
        if (contReg != nullptr)
        {
          auto def = contReg->as<PhyRegister>();
          if (def != nullptr)
          {
            flag |= PhyRegMask::GetPhyRegMask(def);
          }
        }
      }
      // 处理指令操作的寄存器
      for (int i = 0; i < inst->GetOperandSize(); i++)
      {
        PhyRegister *reg = nullptr;
        if (auto sR = inst->GetOperand(i)->as<StackRegister>()) // 栈寄存器
          reg = sR->GetReg()->as<PhyRegister>();                // 获取栈寄存器的基址寄存器
        else
          reg = inst->GetOperand(i)->as<PhyRegister>(); // 获取直接物理寄存器

        if (reg != nullptr)
          flag |= PhyRegMask::GetPhyRegMask(reg);
      }
    }
  }
  return flag;
}

void RISCVFunction::printfull()
{
  NamedMOperand::print();
  std::cout << ":\n"; // 添加冒号和换行
  for (auto mbb : *this)
  {
    mbb->printfull();
  }
}

RISCVFrame::RISCVFrame(RISCVFunction *func) {}

StackRegister *RISCVFrame::spill(VirRegister *vir)
{
  if (spillmap.find(vir) == spillmap.end())
  {
    frameobjs.emplace_back(std::make_unique<RISCVFrameObject>(vir));
    spillmap[vir] = frameobjs.back().get()->GetStackReg(); // 记录该虚拟寄存器对应的栈位置
  }
  return spillmap[vir];
}

RISCVMIR *RISCVFrame::spill(PhyRegister *phy)
{
  int type = phy->Getregenum();

  // 创建新的栈帧对象,存储该寄存器
  frameobjs.emplace_back(std::make_unique<RISCVFrameObject>(phy));
  StackRegister *newStackReg = frameobjs.back().get()->GetStackReg();

  RISCVMIR *store;
  // 整型
  if (type >= PhyRegister::begin_normal_reg && type <= PhyRegister::end_normal_reg)
  {
    store = new RISCVMIR(RISCVMIR::_sd);
  }
  // 浮点型
  else if (type >= PhyRegister::begin_float_reg && type <= PhyRegister::end_float_reg)
  {
    store = new RISCVMIR(RISCVMIR::_fsd);
  }
  else
    assert(0 && "wrong phyregister type!!! no int no float");

  // 源地址 和 目的地址
  store->AddOperand(phy);
  store->AddOperand(newStackReg);

  return store;
}

RISCVMIR *RISCVFrame::load_to_preg(StackRegister *stackReg, PhyRegister *phy)
{
  int type = phy->Getregenum();
  RISCVMIR *load;
  if (type >= PhyRegister::begin_normal_reg && type <= PhyRegister::end_normal_reg)
  {
    load = new RISCVMIR(RISCVMIR::_ld);
  }
  else if (type >= PhyRegister::begin_float_reg && type <= PhyRegister::end_float_reg)
  {
    load = new RISCVMIR(RISCVMIR::_fld);
  }
  else
    assert(0 && "wrong phyregister type");
  load->SetDef(phy);
  load->AddOperand(stackReg);
  return load;
}

void RISCVFrame::GenerateFrame()
{
  using FramObj = std::vector<std::unique_ptr<RISCVFrameObject>>;
  frame_size = 16;
  // 升序排序栈对象的大小(先排列小的)
  std::sort(frameobjs.begin(), frameobjs.end(), [](const std::unique_ptr<RISCVFrameObject> &lhs, const std::unique_ptr<RISCVFrameObject> &rhs)
            { return lhs->GetFrameObjSize() < rhs->GetFrameObjSize(); });
  for (FramObj::iterator it = frameobjs.begin(); it != frameobjs.end(); it++)
  {
    std::unique_ptr<RISCVFrameObject> &contentObj = *it;
    contentObj->SetEndAddOffsets(frame_size);
    frame_size += contentObj->GetFrameObjSize();
    contentObj->SetBeginAddOffsets(frame_size);
  }

  frame_size += parent->GetMaxParamSize(); // 再加上参数区大小
  int mod = frame_size % 16;
  if (mod != 0)
    frame_size = frame_size + (16 - mod);

  for (FramObj::iterator it = frameobjs.begin(); it != frameobjs.end(); it++)
  {
    std::unique_ptr<RISCVFrameObject> &obj = *it;
    int off = 0 - (int)(obj->GetBeginAddOffsets());
    obj->GenerateStackRegister(off);
  }
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
    usedGlobals[val] = mir->GetDef()->as<VirRegister>(); // Allocates the virtual register map table
    auto entryblock = GetFront();                        // Insert the register into the mapping base block
    entryblock->push_front(mir);
  }
  return usedGlobals[val];
}
