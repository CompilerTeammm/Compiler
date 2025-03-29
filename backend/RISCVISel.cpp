#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/"

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

      // constant process
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
        store->AddOperand(M(inst->GetOperand(index)));
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
        ctx();
      else
        ctx();
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
  ctx();
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

    if (curoperand->isConst())
    {
      if (auto nextcuroperand = dynamic_cast<ConstIRInt *>(curoperand))
        offset += size * (size_t)nextcuroperand->GetVal();
      else
        assert("the second isnot const");
    }
    //
    else
    {
    }
  }
  if ()
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
