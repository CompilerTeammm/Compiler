#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/RISCVRegister.hpp"

RISCVISel::RISCVISel(RISCVLoweringContext &_ctx, RISCVAsmPrinter *&asmprinter) : ctx(_ctx), asmprinter(asmprinter) : ctx(_ctx), asmprinter(asmprinter) {};

bool RISCVISel::run(Funciton *m)
{
  // there are parameters in the function
  if (m->GetParams().size() != 0)
  {
    RISCVBasicBlock *entry = RISCVBasicBlock::CreateRISCVBasicBlock();
    RISCVLoweringContext &ctx = this->ctx;
    ctx(entry);

    LowerFormalArguments(m, ctx);
    ///@todo
    ctx.mapping(m->front())->as<RISCVBasicBlock>();
    RISCVMIR *uncondinst = new RISCVMIR(RISCVMIR::RISCVISA::_j);
    uncondinst->AddOperand(ctx.mapping(m->front())->as<RISCVBasicBlock>());
    entry->push_back(uncondinst);
  }

  /// IR->Opt
  auto AM = _AnalysisManager();
  auto mdom = AM.get<dominance>(m);

  for (auto i : mdom->DFG_Dom())
  {
    auto bb = i->thisBlock;
    ctx(ctx->mapping(bb)->as<RISCVBasicBlock>());

    for (auto inst : *bb)
      InstLowering(inst);
  }

  // other
}

void RISCVISel::InstLowering(Instruction *inst)
{
  if (auto store = dynamic_cast<LoadInst *>(inst))
    InstLowering(store);
  if (auto alloca = dynamic_cast<AllocaInst *>(inst))
    InstLowering(alloca);
  if (auto call = dynamic_cast<CallInst *>(inst))
    InstLowering(call);
  if (auto ret = dynamic_cast<RetInst *>(inst))
    InstLowering(ret);
  if (auto cond = dynamic_cast<CondInst *>(inst))
    InstLowering(cond);
  if (auto uncond = dynamic_cast<UnCondInst *>(inst))
    InstLowering(uncond);
  if (auto binary = dynamic_cast<BinaryInst *>(inst))
    InstLowering(binary);
  if (auto zext = dynamic_cast<ZextInst *>(inst))
    InstLowering(zext);
  if (auto sext = dynamic_cast<SextInst *>(inst))
    InstLowering(sext);
  if (auto trunc = dynamic_cast<TruncInst *>(inst))
    InstLowering(trunc);
  if (auto max = dynamic_cast<MaxInst *>(inst))
    InstLowering(max);
  if (auto min = dynamic_cast<MinInst *>(inst))
    InstLowering(min);
  if (auto sel = dynamic_cast<SelectInst *>(inst))
    InstLowering(sel);
  if (auto gep = dynamic_cast<GepInst *>(inst))
    InstLowering(gep);
  if (auto fp2si = dynamic_cast<FP2SIInst *>(inst))
    InstLowering(fp2si);
  if (auto si2fp = dynamic_cast<SI2FPInst *>(inst))
    InstLowering(si2fp);
  if (auto phi = dynamic_cast<PhiInst *>(inst))
    InstLowering(phi);
  else
    assert(0 && "Invalid Inst Type");
}

void RISCVISel::InstLowering(LoadInst *inst)
{
  // int
  if (inst->GetType() == IntType::NewIntTypeGet())
  {
    ctx(Builder(RISCVMIR::_lw, inst));
  }
  // float
  else if (inst->GetType() == FloatType::NewFloatTypeGet())
  {
    ctx(Builder(RISCVMIR::_flw, inst));
  }
  //
  else if (PointerType *ptrtype = dynamic_cast<PointerType *>(inst->GetType()))
  {
    ctx(Builder(RISCVMIR::_ld, inst));
  }
  else
    assert(0 && "invalid load type");
}

void RISCVISel::InstLowering(AllocaInst *inst)
{
  ctx.mapping(inst);
}

void RISCVISel::InstLowering(CallInst *inst)
{
  // Parallel
  Function *called_midfunc;
  RISCVFunction *called_func;

  std::vector<int> spillnodes;

  BuiltinFunc *buildin_func;

  // Function | BuiltinFunc ——> RISCVFunction
  if (called_midfunc = dynamic_cast<Function *>(inst->GetOperand(0)))
  {
    called_func = dynamic_cast<RISCVFunction *>(ctx.mapping(called_midfunc));
    spillnodes = called_func->GetParamNeedSpill();
  }
  else
  {
    buildin_func = dynamic_cast<BuiltinFunc *>(inst->GetOperand(0));
    called_func = dynamic_cast<RISCVFunction *>(ctx.mapping(buildin_func));
    spillnodes = called_func->GetParamNeedSpill();
  }

  if (!inst->Getuselist().empty())
  {
    int offset = 0;
    size_t local_param_size = 0;
    for (auto it = spillnodes.rbegin(); it != spillnodes.rend(); it++)
    {
      int index = *it + 1;
      RISCVMIR *store = nullptr;
      StackRegister *sreg = nullptr;

      // Parameter passing
      switch (RISCVTyper(inst->GetOperand(index)->GetType()))
      {
      case riscv_i32:
        sreg = new StackRegister(PhyRegister::sp, offset);
        store = new RISCVMIR(RISCVMIR::_sw);
        offset += 4;
        break;
      case riscv_float32:
        sreg = new StackRegister(PhyRegister::sp, offset);
        store = new RISCVMIR(RISCVMIR::_fsw);
        offset += 4;
        break;
      case riscv_ptr:
        sreg = new StackRegister(PhyRegister::sp, offset);
        store = new RISCVMIR(RISCVMIR::_sd);
        offset += 8;
        break;
      default:
        assert(0 && "Error param type,shitbro");
      }

      // constant process to stack memory
      // no float ：int32 int64
      if (inst->GetOperand(index)->isConst() && inst->GetOperand(index)->GetType() != FloatType::NewFloatTypeGet())
      {
        auto li = new RISCVMIR(RISCVMIR::li);
        auto vreg = ctx.createVReg(RISCVTyper(inst->GetOperand(index)->GetType()));
        li->SetDef(vreg);
        li->AddOperand(Imm::GetImm(inst->GetOperand(index)->as<ConstantData>()));
        ctx(li);
        store->AddOperand(vreg);
      }
      else
      {
        store->AddOperand(ctx.mapping(inst->GetOperand(index)));
      }
      store->AddOperand(sreg);
      ctx(store);

      local_param_size += op->GetType()->get_size();
    }

    if ()
  }
}

void RISCVISel::InstLowering(StoreInst *inst)
{
  if (inst->GetOperand(0)->GetType() == IntType::NewIntTypeGet())
  {
    if (ConstIRInt *IntConst = dynamic_cast<ConstIRInt *>(inst->GetOperand(0)))
    {
      auto minst = new RISCVMIR(RISCVMIR::_sw);
      minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(Intconst)));
      minst->AddOperand(ctx.mapping(op1));
      ctx(minst);
    }
    else
      ctx(Builder_withoutDef(RISCVMIR::_sw, inst));
  }
  else if (inst->GetOperand(0)->GetType() == FloatType::NewFloatTypeGet())
  {
    ctx(Builder_withoutDef(RISCVMIR::_fsw, inst));
  }
  else
    assert(0 && "fk invalid store type");
}

void RISCVISel::InstLowering(RetInst *inst)
{
  // If empty or undefined value
  if (inst->GetUserUseList().empty() || inst->GetOperand(0)->inUndefval())
  {
    auto minst = new RISCVMIR(RISCVMIR::ret);
    ctx(minst); // Pass it into the new module
  }
  // int
  else if (inst->GetOperand(0) && inst->GetOperand(0)->GetType() == IntType::NewIntTypeGet())
  {
    if (inst->GetOperand(0)->isConst())
    {
      ctx();
    }
    else
    {
      ctx();
    }
    auto minst = new RISCVMIR(RISCVMIR::ret);
    minst->AddOperand(PhyRegister::GetPhyReg(PhyRegister::PhtReg::a0));
    ctx(minst);
  }

  // float
  else if (inst->GetOperand(0) && inst->GetOperand(0)->GetType() == FloatType::NewFloatTypeGet())
  {
    ctx();
  }
  else
  {
    auto minst = new RISCVMIR(RISCVMIR::ret);
    minst->AddOperand(PhyRegister::GetPhyReg(PhyRegister::PhtReg::fa0));
    ctx(minst);
  }
  else assert(0 && "Invalid return type");
}

void RISCVISel::InstLowering(CondInst *inst)
{
  // const bool     eg:if(true)
  if (auto cond = inst->GetOperand(0)->as<ConstIRBoolean>)
  {
    bool condition = cond->GetVal();
    if (condition)
    {
      ctx(Builder_withoutDef(RISCVMIR::_j, {ctx.mapping(inst->GetOperand(1))}));
    }
    else
    {
      ctx(Builder_withoutDef(RISCVMIR::_j, {ctx.mapping(inst->GetOperand(2))}));
    }
    return;
  }
  // Conditional comparison   eg:if( a > b )
  else if (auto cond = inst->GetOperand(0)->as<BinaryInst>)
  {
    assert(cond->GetDef()->GetType() == BoolType::NewBoolTypeGet() && "Invalid Condition Type");

    bool onlyUser = cond->GetUserListSize() == 1;

    // float type
    if (onlyUser && cond->GetOperand(0)->GetType() != FloatType::NewFloatTypeGet())
    {
      auto opcode = cond->getoperation(); // no finish？？？？？
      // lambda function
      auto cond_opcodes = [&](RISCVMIR::RISCVISA opcode)
      {
        ctx(Builder_withoutDef(opcode, {ctx.mapping(cond->GetOperand(0)), ctx.mapping(cond->GetOperand(1)), ctx.mapping(inst->GetOperand(1))}));
        ctx(Builder_withoutDef(RISCVMIR::_j, {ctx.mapping(inst->GetOperand(2))}));
      };
      swtch(opcode)
      {
      case BinaryInst::Op_L:
      {
        // <
        cond_opcodes(RISCVMIR::_blt);
        break;
      }
      case BinaryInst::Op_LE:
      {
        // <=
        cond_opcodes(RISCVMIR::_ble);
        break;
      }
      case BinaryInst::Op_G:
      {
        // >
        cond_opcodes(RISCVMIR::bgt);
        break;
      }
      case BinaryInst::Op_GE:
      {
        // >=
        cond_opcodes(RISCVMIR::_bge);
        break;
      }
      case BinaryInst::Op_E:
      {
        // ==
        cond_opcodes(RISCVMIR::_beq);
        break;
      }
      case BinaryInst::Op_NE:
      {
        // !=
        cond_opcodes(RISCVMIR::_bne);
        break;
      }
      case BinaryInst::Op_And:
      case BinaryInst::Op_Or:
      case BinaryInst::Op_Xor:
      {
        // && || !
        ctx(Builder_withoutDef(RISCVMIR::_bne, {ctx.mapping(cond->GetDef()), PhyRegister::GetPhyReg(PhyRegister::zero), ctx.mapping(inst->GetOperand(1))}));
        ctx(Builder_withoutDef(RISCVMIR::_j, {ctx.mapping(inst->GetOperand(2))}));
        break;
      }
      default:
        assert(0 && "Impossible");
        break;
      }
      return;
    }
  }
  // int type
  else
  {
    ctx(Builder_withoutDef(RISCVMIR::_bne, {ctx.mapping(inst->GetOperand(0)), PhyRegister::GetPhyReg(PhyRegister::PhyReg::zero), ctx.mapping(inst->GetOperand(1))}));
    ctx(Builder_withoutDef(RISCVMIR::_j, {ctx.mapping(inst->GetOperand(2))}));
  }
}

void RISCVISel::InstLowering(UnCondInst *inst)
{
  ctx(Builder_withoutDef(RISCVMIR::_j, {ctx.mapping(inst->GetOperand(0))}));
}

void RISCVISel::InstLowering(BinaryInst *inst)
{
  // Both operands cannot be constants
  assert(!(inst->GetOperand(0)->isConst()) && (inst->GetOperand(1)->isConst()));
  Operand temp = inst->GetOperand(0);
  switch (inst->getoperation())
  {
  case BinaryInst::Op_Add:
  {
    // 原子操作？？？

    // 32位int
    if (inst->GetType() == IntType::NewIntTypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
        ctx(Builder(RISCVMIR::_addiw, inst));
      else
        ctx(Builder(RISCVMIR::_addw, inst));
    }
    // 64位int
    else if (inst->GetType() == Int64Type::NewInt64TypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
        ctx(Builder(RISCVMIR::_addi, inst));
      else
        ctx(Builder(RISCVMIR::_add, inst));
    }
    // float
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
    {
      ctx(Builder(RISCVMIR::_fadd_s, inst));
    }
    else
      assert("illegal");
    break;
  }
  case BinaryInst::Op_Sub:
  {
    if (inst->GetType() == IntType::NewIntTypeGet())
    {
      // Immediate subtraction instruction 32int
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
      {
        auto minst = new RISCVMIR(RISCVMIR::_subw);          // create a instruction
        minst->SetDef(ctx.mapping(inst->GetDef()));          // increase the register
        minst->AddOperand(ctx.mapping(inst->GetOperand(0))); // the first operand
        minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
        ctx(minst);
      }
      else
        ctx(Builder(RISCVMIR::_subw, inst));
    }
    // 64int
    else if (inst->GetType() == Int64Type::NewInt64TypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
      {
        auto minst = new RISCVMIR(RISCVMIR::_sub);
        minst->SetDef(ctx.mapping(inst->GetDef()));
        minst->AddOperand(ctx.mapping(inst->GetOperand(0)));
        minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
        ctx(minst);
      }
      else
        ctx(Builder(RISCVMIR::_sub, inst));
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
      ctx(Builder(RISCVMIR::_fsub_s, inst));
    else
      assert("Illegal");
    break;
  }
  case BinaryInst::Op_Mul:
  {
    // the first ：32int 64int
    if (inst->GetType() == IntType::NewIntTypeGet() || inst->GetType() == Int64Type::NewInt64TypeGet())
    {
      // the second ：if int？
      if (ConstIRInt *constint = dynamic_cast<CondInst *>(inst->GetOperand(1)))
      {
        auto i32val = constint->GetVal();

        // Pass 0 virtual register
        if (i32val == 0)
        {
          ctx.insert_val2mop(inst, PhyRegister::GetPhyReg(PhyRegister::zero));
          return;
        }
        //%result = sub &a，2  %a和%result共用一个物理寄存器
        else if (i32val == 1)
        {
          ctx.insert_val2mop(inst, ctx.mapping(inst->GetOperand(0)))
        }
        // Other optimizations
        else
        {
        }
        // only int32
        if (inst->GetType() == IntType::NewIntTypeGet())
          minst = new RISCVMIR(RISCVMIR::_mulw);
        // only int64
        else if (inst->GetType() == Int64Type::NewInt64TypeGet())
          minst = new RISCVMIR(RISCVMIR::_mul);

        minst->SetDef(ctx.mapping(inst->GetDef()));
        minst->AddOperand(ctx.mapping(inst->GetOperand(0)));
        minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
        ctx(minst);
      }
      // The second operand is not a constant
      else
      {
        // second：int32
        if (inst->GetType() == IntType::NewIntTypeGet())
          ctx(Builder(RISCVMIR::_mulw, inst));
        // second：only int64
        else if (inst->GetType() == Int64Type::NewInt64TypeGet())
          ctx(Builder(RISCVMIR::_mul, inst));
        else
          assert(0 && "Error Type!");
      }
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
      ctx(Builder(RISCVMIR::_fmul_s, inst));
    else
      assert("Illegal!");
    break;
  }
  case BinaryInst::Op_Div:
  {
    if (inst->GetType() == IntType::NewIntTypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
      {
        auto i32val = constint->GetVal();
        if (i32val == 0)
        {
          assert(0 && "Int Div Zero, 666");
          return;
        }
        else if (i32val == 1)
        {
          ctx.insert_val2mop(inst, ctx.mapping(inst->GetOperand(0)));
          return;
        }
        // Other optimizations
        else
        {
        }

        auto minst = new RISCVMIR(RISCVMIR::_divw);

        minst->SetDef(ctx.mapping(inst->GetDef()));
        minst->AddOperand(ctx.mapping(inst->GetOperand(0)));
        minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
        ctx(minst);
      }
      else
        ctx(Builder(RISCVMIR::_divw, inst));
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
      ctx(Builder(RISCVMIR::_fdiv_s, inst));
    else
      assert("Illegal!");
    break;
  }
  case BinaryInst::Op_Mod:
  {
    if (inst->GetType() == IntType::NewIntTypeGet() || inst->GetType() == Int64Type::NewInt64TypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
      {
        auto i32val = constint->GetVal();
        // Other optimizations
        RISCVMIR *minst = nullptr;
        // 32int
        if (inst->GetType() == IntType::NewIntTypeGet())
          minst = new RISCVMIR(RISCVMIR::_remw);
        // 64int
        else if (inst->GetType() == Int64Type::NewInt64TypeGet())
          minst = new RISCVMIR(RISCVMIR::_rem);

        minst->SetDef(ctx.mapping(inst->GetDef()));
        minst->AddOperand(ctx.mapping(inst->GetOperand(0)));
        minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
        ctx(minst);
      }
      // second no constant
      else
      {
        if (inst->GetType() == IntType::NewIntTypeGet())
          ctx(Builder(RISCVMIR::_remw, inst));
        else if (inst->GetType() == Int64Type::NewInt64TypeGet())
          ctx(Builder(RISCVMIR::_rem, inst));
        else
          assert(0 && "Error Type!");
      }
      else assert(0 && "Illegal!");
    }
  }
  case BinaryInst::Op_L:
  case BinaryInst::Op_G:
  case BinaryInst::Op_LE:
  case BinaryInst::Op_GE:
  case BinaryInst::Op_E:
  case BinaryInst::Op_NE:
  {
    if (inst->GetUserlist().is_empty())
      break;
    bool onlyuser = inst->GetUserListSize() == 1; // only one user

    bool condinst = false;
    for (auto us : inst->GetUserListSize()) // userlist in instruction
    {
      // If all users are CondInst, then condinst = true
      // If there is at least one user that is not a CondInst, then condinst may be false
      auto user = us->GetUser();
      if (user->as<CondInst>() == nullptr)
        break;
      else
        condinst = true;
    }
    if (!(onlyuser == true && condinst == true && inst->GetOperand(0)->GetType() != FloatType::NewFloatTypeGet()))
      condition_helper(inst);
    break;
  }
  case BinaryInst::Op_And:
  {
    if (inst->GetType() != FloatType::NewFloatTypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
        ctx(Builder(RISCVMIR::_andi, inst));
      else
        ctx(Builder(RISCVMIR::_and, inst));
    }
    else
      assert(0 && "Illegal!");
    break;
  }
  case BinaryInst::Op_Or:
  {
    if (inst->GetType() != FloatType::NewFloatTypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
        ctx(Builder(RISCVMIR::_ori, inst));
      else
        ctx(Builder(RISCVMIR::_or, inst));
    }
    else
      assert(0 && "Illegal!");
    break;
  }
  case BinaryInst::Op_Xor:
  {
    if (inst->GetType() != FloatType::NewFloatTypeGet())
    {
      if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(inst->GetOperand(1)))
        ctx(Builder(RISCVMIR::_xori, inst));
      else
        ctx(Builder(RISCVMIR::_xor, inst));
    }
    else
      assert(0 && "Illegal!");
    break;
  }
  default:
    assert(0 && "Invalid Opcode");
  }
}

void RISCVISel::InstLowering(ZextInst *zext)
{
  auto mreg = ctx.mapping(zext->GetOperand(0));
  ctx.insert_val2mop(zext, mreg);
}

void RISCVISel::InstLowering(SextInst *sext)
{
  auto mreg = ctx.mapping(sext->GetOperand(0));
  ctx.insert_val2mop(sext, mreg);
}

void RISCVISel::InstLowering(TruncInst *trunc)
{
  ctx(Builder(RISCVMIR::_sext_w, trunc));
}

void RISCVISel::InstLowering(MaxInst *max)
{
  assert(!(max->GetOperand(0)->isConst() && max->GetOperand(1)->isConst()));

  if (max->GetType() == IntType::NewIntTypeGet())
  {
    if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(max->GetOperand(1)))
    {
      auto minst = new RISCVMIR(RISCVMIR::_max);
      minst->SetDef(ctx.mapping(max->GetDef()));
      minst->AddOperand(ctx.mapping(max->GetOperand(0)));
      minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
      ctx(minst);
    }
    else
    {
      ctx();
    }
  }
  else if (max->GetType() == FloatType::NewFloatTypeGet())
    ctx();
  else
    assert(0 && "Invalid type");
}

void RISCVISel::InstLowering(MinInst *)
{
  assert(!(min->GetOperand(0)->isConst() && min->GetOperand(1)->isConst()));
  if (min->GetType() == IntType::NewIntTypeGet())
  {
    if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(min->GetOperand(1)))
    {
      auto minst = new RISCVMIR(RISCVMIR::_min);
      minst->SetDef(ctx.mapping(min->GetDef()));
      minst->AddOperand(ctx.mapping(min->GetOperand(0)));
      minst->AddOperand(ctx.GetCurFunction()->GetUsedGlobalMapping(Imm::GetImm(constint)));
      ctx(minst);
    }
    else
      ctx();
  }
  else if (min->GetType() == FloatType::NewFloatTypeGet())
    ctx();
  else
    assert(0 && "Invalid type");
}

void RISCVISel::InstLowering(SelectInst *)
{
  auto trueblock = RISCVBasicBlock::CreateRISCVBasicBlock();
  auto falseblock = RISCVBasicBlock::CreateRISCVBasicBlock();
  auto nextblock = RISCVBasicBlock::CreateRISCVBasicBlock();

  auto dstVreg = ctx.createVReg(RISCVTyper(inst->GetType()));

  ctx(Builder_withoutDef(RISCVMIR::_bne, {ctx.mapping(inst->GetOperand(0)), PhyRegister::GetPhyReg(PhyRegister::PhyReg::zero), trueblock}));
  ctx(Builder_withoutDef(RISCVMIR::_j, {falseblock}));

  {
    ctx(trueblock);
    ctx(RISCVTrival::CopyFrom(dstVreg, ctx.mapping(inst->GetOperand(1))));
    ctx(Builder_withoutDef(RISCVMIR::_j, {nextblock}));
  }

  {
    ctx(falseblock);
    ctx(RISCVTrival::CopyFrom(dstVreg, ctx.mapping(inst->GetOperand(2))));
    ctx(Builder_withoutDef(RISCVMIR::_j, {nextblock}));
  }

  ctx(nextblock);
}

void RISCVISel::InstLowering(GepInst *inst)
{
  int UserPtrs = inst->GetUserUseList().size(); // operands
  auto hassubtype = dynamic_cast<HasSubType *>(inst->GerOperand(0)->GetType());
  size_t offset = 0;

  for (int i = 1; i < UserPtrs; i++)
  {
    assert(hassubtype != nullptr && "hassubtype is null");

    size_t size = hassubtype->GetSubType()->get_size();
    auto curoperand = inst->GetOperand(i);
    VirRegister *vreg = nullptr;

    // 常量索引
    if (curoperand->isConst())
    {
      if (auto nextcuroperand = dynamic_cast<ConstIRInt *>(curoperand))
        offset += size * (size_t)nextcuroperand->GetVal();
      else
        assert("the second isnot const");
    }
    // 非常量索引(待定)
    else
    {
      // 优化1：使用更高效的2的幂次方检测方法
      RISCVMIR *minst = nullptr;
      bool is_power_of_two = (size > 0) && ((size & (size - 1)) == 0);

      if (is_power_of_two)
      {
        // 优化2：使用内置函数或高效算法计算log2
        int shift = 0;
        for (int tmp = size; tmp > 1; tmp >>= 1)
        {
          shift++;
        }

        minst = Builder(RISCVMIR::_slli, {ctx.createVReg(RISCVType::riscv_ptr), M(index), Imm::GetImm(ConstIRInt::GetNewConstant(shift))});
        ctx(minst);
      }
      else
      {
        // 优化3：提前获取全局映射减少重复调用
        auto size_imm = Imm::GetImm(ConstIRInt::GetNewConstant(size));
        auto global_mapping = ctx.GetCurFunction()->GetUsedGlobalMapping(size_imm);

        minst = Builder(RISCVMIR::_mul, {ctx.createVReg(RISCVType::riscv_ptr), M(index), global_mapping});
        ctx(minst);
      }

      // 优化4：统一寄存器处理逻辑
      VirRegister *index_reg = minst->GetDef()->as<VirRegister>();
      if (vreg != nullptr)
      {
        auto newreg = ctx.createVReg(RISCVType::riscv_ptr);
        ctx(Builder(RISCVMIR::_add, {newreg, vreg, index_reg}));
        vreg = newreg;
      }
      else
      {
        vreg = index_reg;
      }
    }
  }
  if (auto normal = ctx.mapping(inst->GetOperand(0))->as<VirRegister>())
  {
    VirRegister *result_reg = nullptr;

    if (vreg == nullptr)
    {
      // Case 1: base + offset
      result_reg = (offset != 0) ? ctx.createVReg(RISCVType::riscv_ptr) : normal;
      if (offset != 0)
      {
        ctx(Builder(RISCVMIR::_addi, {result_reg, normal, Imm::GetImm(ConstIRInt::GetNewConstant(offset))}));
      }
    }
    else
    {
      // Case 2: base + index + offset
      result_reg = ctx.createVReg(RISCVType::riscv_ptr);
      ctx(Builder(RISCVMIR::_add, {result_reg, normal, vreg}));
      if (offset != 0)
      {
        ctx(Builder(RISCVMIR::_addi, {result_reg, result_reg, Imm::GetImm(ConstIRInt::GetNewConstant(offset))}));
      }
    }
    ctx.insert_val2mop(inst, result_reg);
  }
  else
  {
    assert(0 && "Expected VirRegister for address calculation");
  }
}

void RISCVISel::InstLowering(FP2SIInst *inst)
{
  ctx(Builder(RISCVMIR::_fcvt_w_s, inst));
}

void RISCVISel::InstLowering(SI2FPInst *inst)
{
  ctx(RISCVMIR::_fcvt_s_w, inst);
}

void RISCVISel::InstLowering(PhiInst *inst)
{
  return;
}

RISCVMIR *Builder(RISCVMIR::RISCVISA _isa, Instruction *inst)
{
  auto minst = new RISCVMIR(_isa);
  minst->SetDef(ctx.mapping(inst));
  for (int i = 0; i < inst->GetUserUseList.size(); i++)
  {
    minst->AddOperand(ctx.mapping(inst->GetOperand(i)));
  }
  return minst;
}

RISCVMIR *RISCVISel::Builder_withoutDef(RISCVMIR::RISCVISA _isa, Instruction *inst)
{
  auto minst = new RISCVMIR(_isa);
  for (int i = 0; i < inst->Getuselist().size(); i++)
    minst->AddOperand(ctx.mapping(inst->GetOperand(i)));
  return minst;
}

RISCVMIR *Builder(RISCVMIR::RISCVISA, std::initializer_list<RISCVMOperand *>)
{
  auto minst = new RISCVMIR(_isa);
  minst->SetDef(list.begin()[0]);
  for (auto it = list.begin() + 1; it != list.end(); ++it)
  {
    RISCVMOperand *i = *it;
    minst->AddOperand(i);
  }
  return minst;
}

RISCVMIR *RISCVISel::Builder_withoutDef(RISCVMIR::RISCVISA _isa, std::initializer_list<RISCVMOperand *> list)
{
  auto minst = new RISCVMIR(_isa);
  for (auto it = list.begin(); it != list.end(); ++it)
  {
    RISCVMOperand *i = *it;
    minst->AddOperand(i);
  }
  return minst;
}

void RISCVISel::LowerCallInstParallel(CallInst *inst)
{
  auto func_called = inst->GetOperand(0)->as<Function>();
  assert(func_called != nullptr);

  // prepare
  auto createMir = [&](PhyRegister *preg, RISCVMIR::RISCVISA _opcode, Operand op0) -> VirRegister *
  {
    auto mir = new RISCVMIR(_opcode);     // Create new MIR with given opcode
    auto def = ctx.createVReg(riscv_i32); // Create virtual register
    mir->SetDef(preg);                    // Set physical register definition
    mir->AddOperand(ctx.mapping(op0));    // Add mapped operand
    ctx(mir);                             // Add MIR to context
    return def;                           // Return virtual register
  };

  // Generate a RISC-V instruction to load the address of the global symbol 'tag' into the virtual register 'reg'
  auto loadTagAddress = [&](VirRegister *reg, std::string tag)
  {
    auto addressinst = new RISCVMIR(RISCVMIR::LoadGlobalAddr);
    addressinst->SetDef(reg);
    auto args_storage = OuterTag::GetOuterTag(tag);
    addressinst->AddOperand(args_storage);
    ctx(addressinst);
  };

  // Obtain a function pointer and load its address
  auto funcptr = ctx.createVReg(riscv_ptr);        // create XUNI Vir
  loadTagAddress(funcptr, func_called->GetName()); // load ptr

  // Load the address of the function pointer storage area
  auto funcptrstorage = ctx.createVReg(riscv_ptr);
  loadTagAddress(funcptrstorage, "buildin_funcptr");

  // Store the function pointer to the target location
  auto sd2funcptr = new RISCVMIR(RISCVMIR::_sd);
  sd2funcptr->AddOperand(funcptr);
  sd2funcptr->AddOperand(funcptrstorage);
  ctx(sd2funcptr);

  // Load the address of the parallel parameter storage area
  auto addressreg = ctx.createVReg(riscv_ptr);
  loadTagAddress(addressreg, "buildin_parallel_arg_storage");

  auto mvaddress = [&](int offset)
  {
    auto addi = new RISCVMIR(RISCVMIR::_addi);
    addi->SetDef(addressreg);
    addi->AddOperand(addressreg);
    addi->AddOperand(Imm::GetImm(ConstIRInt::GetNewConstant(offset)));
    ctx(addi);
  };

  int offset = 0;
  // 假设最多使用6个寄存器传参（a0-a5），超过部分用栈
  const int MAX_REG_ARGS = 6;
  PhyRegister::PhyRegisterId argRegs[MAX_REG_ARGS] = {
      PhyRegister::a0, PhyRegister::a1, PhyRegister::a2,
      PhyRegister::a3, PhyRegister::a4, PhyRegister::a5};

  for (int i = 1, limi = inst->Getuselist().size(); i < limi; i++)
  {
    auto *operand = inst->GetOperand(i);
    auto *type = operand->GetType();
    bool isFloat = (type == FloatType::NewFloatTypeGet());

    // 寄存器传参（整型/指针优先用aX，浮点用fX）
    if (i <= MAX_REG_ARGS)
    {
      if (!isFloat)
      {
        // 整型/指针参数
        auto phyreg = PhyRegister::GetPhyReg(argRegs[i - 1]);
        if (operand->isConst())
        {
          createMir(phyreg, RISCVMIR::li, operand);
        }
        else
        {
          // 添加寄存器到寄存器移动优化
          if (auto srcReg = dynamic_cast<VirRegister *>(ctx.mapping(operand)))
          {
            if (ctx.isPhysicalReg(srcReg))
            {
              createMir(phyreg, RISCVMIR::mv, operand);
            }
            else
            {
              // 如果源是虚拟寄存器，尝试合并分配
              ctx.tryCoalesce(srcReg, phyreg);
            }
          }
        }
      }
      else
      {
        // 浮点参数（使用fa0-fa7，这里示例用栈处理，实际可优化）
        offset += 4;
        store2place(RISCVMIR::_fsw, ctx.mapping(operand));
      }
    }
    // 栈传参
    else
    {
      auto op = ctx.mapping(operand);
      RISCVMIR::RISCVISA opcode;
      int size;

      if (dynamic_cast<PointerType *>(type))
      {
        opcode = RISCVMIR::_sd;
        size = 8;
      }
      else if (isFloat)
      {
        opcode = RISCVMIR::_fsw;
        size = 4;
      }
      else
      {
        opcode = RISCVMIR::_sw;
        size = 4;
      }

      // 批量处理连续栈存储
      if (offset % size != 0)
      {
        offset = (offset + size - 1) & ~(size - 1); // 对齐
      }
      store2place(opcode, op);
      offset += size;
    }
  }

  // 1. 使用三元运算符简化条件选择
  auto NotifyWorker = func_called->CmpEqual ? BuildInFunction::GetBuildInFunction("buildin_NotifyWorkerLE") : BuildInFunction::GetBuildInFunction("buildin_NotifyWorkerLT");

  // 2. 直接构造 call 指令，避免中间变量
  auto *call = new RISCVMIR(RISCVMIR::call);
  call->AddOperand(M(NotifyWorker));

  // 3. 检查参数是否已正确设置，避免冗余寄存器传递
  if (!ctx.isRegReady(PhyRegister::a0) || !ctx.isRegReady(PhyRegister::a1))
  {
    ctx.loadArgsToRegs({PhyRegister::a0, PhyRegister::a1}); // 批量加载参数到寄存器
  }

  // 4. 添加调用约定要求的寄存器（a0, a1）
  call->AddOperand(PhyRegister::GetPhyReg(PhyRegister::a0));
  call->AddOperand(PhyRegister::GetPhyReg(PhyRegister::a1));

  // 5. 提交指令前检查函数是否存在
  assert(NotifyWorker != nullptr && "NotifyWorker function not found");
  ctx(call);
}

void RISCVISel::LowerCallInstCacheLookUp(CallInst *inst)
{
  this->asmprinter->set_use_cachelookup(true);
  RISCVMIR *call = new RISCVMIR(RISCVMIR::call);

  BuildInFunction *buildinfunc = BuildInFunction::GetBuildInFunction(inst->GetOperand(0)->GetName());
  call->AddOperand(M(buildinfunc));

  int regint = PhyRegister::PhyReg::a0;
  for (int index = 1; index < 3; index++)
  {
    Operand op = inst->GetOperand(index);
    if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(op))
    {
      auto li = new RISCVMIR(RISCVMIR::li);
      PhyRegister *preg = PhyRegister::GetPhyReg(static_cast<PhyRegister::PhyReg>(regint));
      call->AddOperand(preg);
      li->SetDef(preg);
      Imm *imm = Imm::GetImm(constint);
      li->AddOperand(imm);
      ctx.insert_val2mop(constint, imm);
      ctx(li);
      regint++;
    }
    else if (RISCVTyper(op->GetType()) == riscv_i32 || RISCVTyper(op->GetType()) == riscv_ptr)
    {
      PhyRegister *preg = PhyRegister::GetPhyReg(static_cast<PhyRegister::PhyReg>(regint));
      ctx(Builder(RISCVMIR::mv, {preg, M(op)}));
      call->AddOperand(preg);
      regint++;
    }
    else if (RISCVTyper(op->GetType()) == riscv_float32)
    {
      PhyRegister *reg = PhyRegister::GetPhyReg(static_cast<PhyRegister::PhyReg>(regint));
      ctx(Builder(RISCVMIR::_fmv_x_w, {reg, M(op)}));
      call->AddOperand(reg);
      regint++;
    }
    else
      assert(0 && "CallInst selection illegal type");
  }
  ctx(call);
  call->SetDef(PhyRegister::GetPhyReg(PhyRegister::PhyReg::fa0));
  ctx(Builder(RISCVMIR::mv, {ctx.mapping(inst), PhyRegister::GetPhyReg(PhyRegister::PhyReg::a0)}));
}

void RISCVISel::LowerCallInstCacheLookUp4(CallInst *inst)
{
  this->asmprinter->set_use_cachelookup4(true);
  RISCVMIR *call = new RISCVMIR(RISCVMIR::call);
  // the call inst is: call CacheLookUp(int/float, int/float)
  // the size of uselist is 5, and the first use is called function
  BuiltinFunc *buildinfunc = BuildInFunction::GetBuildInFunction(inst->GetOperand(0)->GetName());
  call->AddOperand(M(buildinfunc)); // "CacheLookUp4"
  int regint = PhyRegister::PhyReg::a0;
  for (int index = 1; index < 5; index++)
  {
    Operand op = inst->GetOperand(index);
    if (ConstIRInt *constint = dynamic_cast<ConstIRInt *>(op))
    {
      auto li = new RISCVMIR(RISCVMIR::li);
      PhyRegister *preg = PhyRegister::GetPhyReg(static_cast<PhyRegister::PhyReg>(regint));
      call->AddOperand(preg);
      li->SetDef(preg);
      Imm *imm = Imm::GetImm(constint);
      li->AddOperand(imm);
      ctx.insert_val2mop(constint, imm);
      ctx(li);
      regint++;
    }
    else if (RISCVTyper(op->GetType()) == riscv_i32 || RISCVTyper(op->GetType()) == riscv_ptr)
    {
      PhyRegister *preg = PhyRegister::GetPhyReg(static_cast<PhyRegister::PhyReg>(regint));
      ctx(Builder(RISCVMIR::mv, {preg, M(op)}));
      call->AddOperand(preg);
      regint++;
    }
    else if (RISCVTyper(op->GetType()) == riscv_float32)
    {
      PhyRegister *reg = PhyRegister::GetPhyReg(static_cast<PhyRegister::PhyReg>(regint));
      ctx(Builder(RISCVMIR::_fmv_x_w, {reg, M(op)}));
      call->AddOperand(reg);
      regint++;
    }
    else
      assert(0 && "CallInst selection illegal type");
  }
  ctx(call);
  call->SetDef(PhyRegister::GetPhyReg(PhyRegister::PhyReg::fa0));
  ctx(Builder(RISCVMIR::mv, {ctx.mapping(inst), PhyRegister::GetPhyReg(PhyRegister::PhyReg::a0)}));
}

void RISCVISel::condition_helper(BinaryInst *inst)
{
  assert(inst->GetType() == BoolType::NewBoolTypeGet());
  assert(inst->GetOperand(0)->GetType() == inst->GetOperand(1)->GetType());
  assert(inst->GetOperand(0)->GetType() == BoolType::NewBoolTypeGet() || inst->GetOperand(0)->GetType() == IntType::NewIntTypeGet() || inst->GetOperand(1)->GetType() == FloatType::NewFloatTypeGet());
  bool isint = (inst->GetOperand(0)->GetType() != FloatType::NewFloatTypeGet());
  switch (inst->getopration())
  {
  case BinaryInst::Op_E:
  {
    if (isint)
    {
      // xor vreg0, a, b
      // seqz vreg1, vreg0
      auto vreg0 = ctx.createVReg(riscv_i32);
      auto vreg1 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg1);
      ctx(Builder(RISCVMIR::_xor, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
      ctx(Builder(RISCVMIR::_seqz, {vreg1, vreg0}));
    }
    else
    {
      // feq.s vreg0, a, b
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_feq_s, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
    }
    break;
  }
  case BinaryInst::Op_NE:
  {
    if (isint)
    {
      // xor vreg0, a, b
      // snez vreg1, vreg0
      auto vreg0 = ctx.createVReg(riscv_i32);
      auto vreg1 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg1);
      ctx(Builder(RISCVMIR::_xor, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
      ctx(Builder(RISCVMIR::_snez, {vreg1, vreg0}));
    }
    else
    {
      // feq.s vreg0, a, b
      // seqz vreg1, vreg0
      auto vreg0 = ctx.createVReg(riscv_i32);
      auto vreg1 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg1);
      ctx(Builder(RISCVMIR::_feq_s, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
      ctx(Builder(RISCVMIR::_seqz, {vreg1, vreg0}));
    }
    break;
  }
  case BinaryInst::Op_G:
  {
    if (isint)
    {
      // slt vreg0, b, a
      // if b<a, vreg0=1, that's a>b
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_slt, {vreg0, ctx.mapping(inst->GetOperand(1)), ctx.mapping(inst->GetOperand(0))}));
    }
    else
    {
      // flt vreg0, b, a
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_flt_s, {vreg0, ctx.mapping(inst->GetOperand(1)), ctx.mapping(inst->GetOperand(0))}));
    }
    break;
  }
  case BinaryInst::Op_GE:
  {
    if (isint)
    {
      // slt vreg0, a, b: if a>=b, vreg0 will be 0
      // seqz vreg1, vreg0: thus vreg1 will be 1
      auto vreg0 = ctx.createVReg(riscv_i32);
      auto vreg1 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg1);
      ctx(Builder(RISCVMIR::_slt, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
      ctx(Builder(RISCVMIR::_seqz, {vreg1, vreg0}));
    }
    else
    {
      // fle vreg0 b, a
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_fle_s, {vreg0, ctx.mapping(inst->GetOperand(1)), ctx.mapping(inst->GetOperand(0))}));
    }
    break;
  }
  case BinaryInst::Op_LE:
  {
    if (isint)
    {
      // slt vreg0, b, a: if a<=b vreg0 will be 0
      // seqz vreg1, vreg0: thus vreg1 will be 1
      auto vreg0 = ctx.createVReg(riscv_i32);
      auto vreg1 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg1);
      ctx(Builder(RISCVMIR::_slt, {vreg0, ctx.mapping(inst->GetOperand(1)), ctx.mapping(inst->GetOperand(0))}));
      ctx(Builder(RISCVMIR::_seqz, {vreg1, vreg0}));
    }
    else
    {
      // fle.s
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_fle_s, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
    }
    break;
  }
  case BinaryInst::Op_L:
  {
    if (isint)
    {
      // slt vreg0, a, b: if a<b, vreg0 will be 1
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_slt, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
    }
    else
    {
      // flt.s
      auto vreg0 = ctx.createVReg(riscv_i32);
      ctx.insert_val2mop(inst->GetDef(), vreg0);
      ctx(Builder(RISCVMIR::_flt_s, {vreg0, ctx.mapping(inst->GetOperand(0)), ctx.mapping(inst->GetOperand(1))}));
    }
    break;
  }
  default:
    assert(0 && "Invalid operation");
  }
}