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

void RISCVMIR::SetDef(RISCVMOperand *def) { this->def = def; }

void RISCVFunction::SetMaxParamSize(size_t size) { max_param_size = size; }
size_t RISCVFunction::GetMaxParamSize()
{
  if (!this)
    return 0;
  return max_param_size;
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

RISCVMIR *RISCVFunction::CreateSpecialUsageMIR(RISCVMOperand *val)
{
  VirRegister *vreg = nullptr;
  RISCVMIR *mir = nullptr;
  // 常量
  if (auto imm = val->as<Imm>())
  {
    if (imm->GetType() == riscv_float32)
      vreg = new VirRegister(riscv_float32);
    else
      vreg = new VirRegister(imm->GetType(), 0, 2);
    mir = new RISCVMIR(RISCVMIR::LoadImmReg);
  }
  // 局部变量(数组)
  else if (val->as<RISCVFrameObject>())
  {
    vreg = new VirRegister(riscv_ptr, 0, 2);
    mir = new RISCVMIR(RISCVMIR::LoadLocalAddr);
  }
  // 全局变量
  else
  {
    vreg = new VirRegister(riscv_ptr, 0, 2);
    mir = new RISCVMIR(RISCVMIR::LoadGlobalAddr);
  }
  if (vreg->GetType() != riscv_float32)
    specialusage_remapping[vreg] = val;
  // Push into the entry block
  // FIXME: Currently we put it before the copy of the lowering local arguments.
  mir->SetDef(vreg);
  mir->AddOperand(val);

  return mir;
}

void RISCVFrame::GenerateFrameHead()
{
  if (!parent || !parent->GetFront())
  {
    std::cerr << "Error: Invalid parent or empty function\n";
    return;
  }
  // 初始化物理寄存器
  using Phy = PhyRegister::PhyReg;
  using ISA = RISCVMIR::RISCVISA;
  PhyRegister *sp = PhyRegister::GetPhyReg(Phy::sp); // 栈指针
  PhyRegister *s0 = PhyRegister::GetPhyReg(Phy::s0); // 帧指针
  PhyRegister *ra = PhyRegister::GetPhyReg(Phy::ra); // 返回地址

  // RISC-V 的 addi 指令的立即数范围为 [-2048, 2047)
  int con_frame_size = frame_size;
  if (frame_size > 2047)
  {
    con_frame_size = frame_size % 2048;
  }

  // addi sp, sp, -con_frame_size
  RISCVMIR *inst_addi = new RISCVMIR(ISA::_addi);
  Imm *imm_addi = new Imm(ConstIRInt::GetNewConstant(0 - con_frame_size));
  inst_addi->SetDef(sp);
  inst_addi->AddOperand(sp);
  inst_addi->AddOperand(imm_addi);

  // addi s0, sp, temp_frame_size
  RISCVMIR *inst_addi2 = new RISCVMIR(ISA::_addi);
  Imm *imm_addi2 = new Imm(ConstIRInt::GetNewConstant(con_frame_size));
  inst_addi2->SetDef(s0);
  inst_addi2->AddOperand(sp);
  inst_addi2->AddOperand(imm_addi2);

  // addi sp, sp, framesize-temp_frame_size
  if (frame_size != con_frame_size)
  {
    RISCVMIR *inst_addi3 = new RISCVMIR(ISA::_addi);
    Imm *imm_addi3 = new Imm(ConstIRInt::GetNewConstant(con_frame_size - frame_size));
    inst_addi3->SetDef(sp);
    inst_addi3->AddOperand(sp);
    inst_addi3->AddOperand(imm_addi3);

    parent->GetFront()->begin().InsertBefore(inst_addi3);
  }

  // sd ra, con_frame_size - 8(sp)
  RISCVMIR *inst_ra = new RISCVMIR(ISA::_sd);
  StackRegister *sp_sd = new StackRegister(Phy::sp, con_frame_size - 8);
  inst_ra->AddOperand(ra);
  inst_ra->AddOperand(sp_sd);

  // sd s0, temp_frame_size-16(sp)
  RISCVMIR *inst3 = new RISCVMIR(ISA::_sd);
  StackRegister *sp_stack3 = new StackRegister(Phy::sp, con_frame_size - 16);
  inst3->AddOperand(s0);
  inst3->AddOperand(sp_stack3);

  parent->GetFront()->begin().InsertBefore(inst_addi2);
  parent->GetFront()->begin().InsertBefore(inst3);
  parent->GetFront()->begin().InsertBefore(inst_ra);
  parent->GetFront()->begin().InsertBefore(inst_addi);
}

void RISCVFrame::GenerateFrameTail()
{
  if (!parent || !parent->GetExit())
  {
    std::cerr << "Invalid function or exit block\n";
    return;
  }
  using PhyReg = PhyRegister::PhyReg;
  using ISA = RISCVMIR::RISCVISA;
  PhyRegister *sp = PhyRegister::GetPhyReg(PhyReg::sp);
  PhyRegister *s0 = PhyRegister::GetPhyReg(PhyReg::s0);
  PhyRegister *ra = PhyRegister::GetPhyReg(PhyReg::ra);

  int temp_frame_size = frame_size;
  RISCVFunction *func = parent;
  auto exit_bb = func->GetExit();

  if (frame_size > 2047)
    temp_frame_size = frame_size % 2048;

  // addi sp, sp, framesize-temp_frame_size
  RISCVMIR *inst0 = new RISCVMIR(ISA::_addi);
  if (temp_frame_size != frame_size)
  {
    Imm *imm0 = new Imm(ConstIRInt::GetNewConstant(frame_size - temp_frame_size));
    inst0->SetDef(sp);
    inst0->AddOperand(sp);
    inst0->AddOperand(imm0);
  }

  // ld ra, temp_frame_size-8(sp)
  RISCVMIR *inst1 = new RISCVMIR(ISA::_ld);
  StackRegister *sp_stack1 = new StackRegister(PhyReg::sp, temp_frame_size - 8);
  inst1->SetDef(ra);
  inst1->AddOperand(sp_stack1);

  // ld s0, temp_frame_size-16(sp)
  RISCVMIR *inst2 = new RISCVMIR(ISA::_ld);
  StackRegister *sp_stack2 =
      new StackRegister(PhyReg::sp, temp_frame_size - 16);
  inst2->SetDef(s0);
  inst2->AddOperand(sp_stack2);

  // addi sp, sp, temp_frame_size
  RISCVMIR *inst3 = new RISCVMIR(ISA::_addi);
  Imm *imm3 = new Imm(ConstIRInt::GetNewConstant(temp_frame_size));
  inst3->SetDef(sp);
  inst3->AddOperand(sp);
  inst3->AddOperand(imm3);

  if (temp_frame_size != frame_size)
  {
    exit_bb->push_back(inst0);
  }
  exit_bb->push_back(inst1);
  exit_bb->push_back(inst2);
  exit_bb->push_back(inst3);
}

void RISCVFunction::GenerateParamNeedSpill()
{
  using ParamPtr = std::unique_ptr<Value>;
  BuiltinFunc *buildin;
  if (buildin = dynamic_cast<BuiltinFunc *>(func))
  {
    param_need_spill = {};
    return;
  }
  int IntMax = 8, FloatMax = 8;
  Function *func = dynamic_cast<Function *>(this->func);
  std::vector<ParamPtr> &params = func->GetParams();
  int index = 0;
  for (auto &i : params)
  {
    if (i->GetType() == FloatType::NewFloatTypeGet())
    {
      if (FloatMax)
      {
        FloatMax--;
      }
      else
      {
        this->param_need_spill.push_back(index);
      }
    }
    // int & ptr type
    else
    {
      if (IntMax)
      {
        IntMax--;
      }
      else
      {
        this->param_need_spill.push_back(index);
      }
    }
    index++;
  }
}

std::vector<int> &RISCVFunction::GetParamNeedSpill()
{
  return this->param_need_spill;
}

void RISCVFrame::AddCantBeSpill(RISCVMOperand *reg)
{
  auto it = std::find(cantbespill.begin(), cantbespill.end(), reg);
  if (it != cantbespill.end())
    return;
  cantbespill.push_back(reg);
}

bool RISCVFrame::CantBeSpill(RISCVMOperand *reg)
{
  auto it = std::find(cantbespill.begin(), cantbespill.end(), reg);
  if (it == cantbespill.end())
    return false;
  else
    return true;
}

void Terminator::RotateCondition()
{
  assert(RISCVMIR::BeginBranch < branchinst->GetOpcode() &&
         branchinst->GetOpcode() < RISCVMIR::EndBranch &&
         branchinst->GetOpcode() != RISCVMIR::_j && "Bro is definitely mad");
  branchinst->SetMopcode(
      static_cast<RISCVMIR::RISCVISA>(branchinst->GetOpcode() ^ 1));
  branchinst->SetOperand(2, falseblock);
  auto it = List<RISCVBasicBlock, RISCVMIR>::iterator(branchinst);
  ++it;
  assert(it != branchinst->GetParent()->end());
  auto nxt_inst = *it;
  assert(nxt_inst->GetOpcode() == RISCVMIR::_j);
  nxt_inst->SetOperand(0, trueblock);
  std::swap(trueblock, falseblock);
}

void RISCVBasicBlock::erase(RISCVMIR *inst)
{
  // 空指针检查
  if (!inst)
  {
    assert(false && "Cannot erase null instruction!");
    return;
  }

  // 检查是否是终止指令
  if (dynamic_cast<Terminator *>(inst))
  {
    assert(false && "Use setTerminator() to modify the terminator!");
    return;
  }

  // 遍历查找并删除指令
  bool found = false;
  for (auto it = begin(); it != end(); ++it)
  {
    if (*it == inst)
    {
      // 调用基类的 erase 方法删除指令
      this->List<RISCVBasicBlock, RISCVMIR>::erase(*it);
      found = true;
      break;
    }
  }

  if (!found)
  {
    assert(false && "Instruction does not belong to this block!");
    return;
  }
}