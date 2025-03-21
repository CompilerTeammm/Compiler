#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVLowering.hpp"

// ��ʼ�� RISCVLoweringContext �� RISCVAsmPrinter
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
  // ?????????????????????--->??
  if (inst->GetUserUseList().empty() || inst->GetOperand(0)->inUndefval())
  {
    auto minst = new RISCVMIR(RISCVMIR::ret);
    ctx(minst); // ??????
  }
  // ????????????int??
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
  // ????????????float??
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
  // bool??
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
  // ????
  else if (auto cond = inst->GetOperand(0)->as<BinaryInst>)
  {
    assert(cond->GetDef()->GetType() == BoolType::NewBoolTypeGet() && "Invalid Condition Type");

    bool onlyUser = cond->GetUserListSize() == 1;

    // ??????????float??
    if (onlyUser && cond->GetOperand(0)->GetType() != FloatType::NewFloatTypeGet())
    {
      auto opcode = cond->getoperation();
      // lambda??
      auto cond_opcodes = [&](RISCVMIR::RISCVISA opcode)
      {
        ctx();
        ctx();
      } swtch(opcode)
      {
      case BinaryInst::Op_L:
      {
        // ??
        cond_opcodes(RISCVMIR::_blt);
        break;
      }
      case BinaryInst::Op_LE:
      {
        // ????
        cond_opcodes(RISCVMIR::_ble);
        break;
      }
      case BinaryInst::Op_G:
      {
        // ??
        cond_opcodes(RISCVMIR::bgt);
        break;
      }
      case BinaryInst::Op_GE:
      {
        // ????
        cond_opcodes(RISCVMIR::_bge);
        break;
      }
      case BinaryInst::Op_E:
      {
        // ??
        cond_opcodes(RISCVMIR::_beq);
        break;
      }
      case BinaryInst::Op_NE:
      {
        // ???
        cond_opcodes(RISCVMIR::_bne);
        break;
      }
      case BinaryInst::Op_And:
      case BinaryInst::Op_Or:
      case BinaryInst::Op_Xor:
      {
        // ????
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
  else
  {
    ctx();
    ctx();
  }
}