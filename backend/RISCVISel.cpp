#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVLowering.hpp"

RISCVISel::RISCVISel(RISCVLoweringContext &_ctx, RISCVAsmPrinter *&asmprinter) : ctx(_ctx), asmprinter(asmprinter) : ctx(_ctx), asmprinter(asmprinter) {};

bool RISCVISel::run(Funciton *m)
{
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
  // ??
  else if (PointerType *ptrtype = dynamic_cast<PointerType *>(inst->GetType()))
  {
    ctx(Builder(RISCVMIR::_ld, inst));
  }
  else
    assert(0 && "invalid load type");
}

void RISCVISel::InstLowering(AllocaInst *inst)
{
  ///@todo
}

void RISCVISel::InstLowering(CallInst *inst)
{
  ///@todo
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

void RISCVISel::InstLowering(CondInst *)
{
  // const bool     eg:if(true)
  if (auto cond = inst->GetOperand(0)->as<ConstIRBoolean>)
  {
    bool condition = cond->GetVal();
    if (condition)
    {
      ctx();
    }
    else
    {
      ctx();
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
        ctx();
        ctx();
      } swtch(opcode)
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
        ctx();
        ctx();
        break;
      }
      default:
        break;
      }
      return;
    }
  }
  // int type
  else
  {
    ctx();
    ctx();
  }
}

void RISCVISel::InstLowering(UnCondInst *)
{
  ctx();
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
        ctx();
      else
        ctx();
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
        ctx();
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
        ctx();
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
      ctx();
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
          ctx();
        // second：only int64
        else if (inst->GetType() == Int64Type::NewInt64TypeGet())
          ctx();
        else
          assert(0 && "Error Type!");
      }
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
      ctx();
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
        ctx();
    }
    else if (inst->GetType() == FloatType::NewFloatTypeGet())
      ctx();
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
          ctx();
        else if (inst->GetType() == Int64Type::NewInt64TypeGet())
          ctx();
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
        ctx();
      else
        ctx();
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
        ctx();
      else
        ctx();
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
}

void RISCVISel::InstLowering(GepInst *inst)
{
}
